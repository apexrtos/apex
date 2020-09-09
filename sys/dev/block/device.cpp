#include "device.h"

#include <algorithm>
#include <debug.h>
#include <device.h>
#include <errno.h>
#include <fs/file.h>
#include <fs/util.h>
#include <kernel.h>
#include <linux/fs.h>
#include <sys/uio.h>
#include <timer.h>

namespace {

int
block_open(file *f)
{
	return static_cast<block::device *>(f->f_data)->open();
}

int
block_close(file *f)
{
	return static_cast<block::device *>(f->f_data)->close();
}

ssize_t
block_read(file *f, const iovec *v, size_t c, off_t o)
{
	return static_cast<block::device *>(f->f_data)->read(v, c, o);
}

ssize_t
block_write(file *f, const iovec *v, size_t c, off_t o)
{
	return static_cast<block::device *>(f->f_data)->write(v, c, o);
}

int
block_ioctl(file *f, unsigned long c, void *a)
{
	return static_cast<block::device *>(f->f_data)->ioctl(c, a);
}

constinit devio block_io{
	.open = block_open,
	.close = block_close,
	.read = block_read,
	.write = block_write,
	.ioctl = block_ioctl,
};

}

namespace block {

/*
 * device::device - block device constructor
 */
device::device(::device *dev, off_t size)
: dev_{dev}
, nopens_{0}
, size_{size}
, buf_{0, {0, nullptr}}
{
	device_attach(dev_, &block_io, DF_BLK, this);
}

/*
 * device::~device - block device destructor
 */
device::~device()
{
	/* hide device node */
	device_hide(dev_);

	/* wait for active operations to complete */
	while (device_busy(dev_))
		timer_delay(10e6);

	/* destroy device */
	device_destroy(dev_);
}

/*
 * device::open - open block device and allocate block buffer
 */
int
device::open()
{
	std::lock_guard l{mutex_};

	if (nopens_++)
		return 0;
	std::unique_ptr<phys> buf{page_alloc(PAGE_SIZE, MA_NORMAL | MA_DMA, this),
	    {PAGE_SIZE, this}};
	if (!buf)
		return DERR(-ENOMEM);
	if (auto r = v_open(); r < 0)
		return r;
	buf_ = std::move(buf);
	off_ = std::numeric_limits<off_t>::max();
	dirty_ = false;
	return 0;
}

/*
 * device::close - close block device and free block buffer
 */
int
device::close()
{
	std::lock_guard l{mutex_};

	assert(nopens_ > 0);

	if (--nopens_)
		return 0;
	sync();
	buf_.reset();
	return v_close();
}

/*
 * device::read - read from block device
 */
ssize_t
device::read(const iovec *iov, size_t count, off_t off)
{
	return transfer(iov, count, off, false);
}

/*
 * device::write - write to block device
 */
ssize_t
device::write(const iovec *iov, size_t count, off_t off)
{
	return transfer(iov, count, off, true);
}

/*
 * device::ioctl - perform i/o control on block device
 */
int
device::ioctl(unsigned long cmd, void *arg)
{
	std::lock_guard l{mutex_};

	assert(nopens_ > 0);

	uint64_t *arg64 = static_cast<uint64_t *>(arg);
	auto valid_range = [&]{
		if (!ALIGNED(arg, uint64_t))
			return DERR(false);
		if (arg64[0] & PAGE_MASK || arg64[1] & PAGE_MASK)
			return DERR(false);
		if (arg64[0] > (uint64_t)size_)
			return DERR(false);
		if (arg64[1] > size_ - arg64[0])
			return DERR(false);
		return true;
	};

	switch (cmd) {
	case BLKDISCARD:
	case BLKSECDISCARD:
		/* discard data, no read guarantees */
		if (!valid_range())
			return DERR(-EINVAL);
		return v_discard(arg64[0], arg64[1], cmd == BLKSECDISCARD);
	case BLKZEROOUT: {
		/* discard data, guarantee read will return zeros */
		if (!valid_range())
			return DERR(-EINVAL);
		if (auto r = v_zeroout(arg64[0], arg64[1]); r != -ENOTSUP)
			return r;

		/* device doesn't support zeroing, allocate a page of zeros */
		std::unique_ptr<phys> p{page_alloc(PAGE_SIZE, MA_NORMAL | MA_DMA, this),
		    {PAGE_SIZE, this}};
		if (!p)
			return DERR(-ENOMEM);
		std::byte *z = static_cast<std::byte *>(phys_to_virt(p.get()));
		memset(z, 0, PAGE_SIZE);

		/* 256 iovs: 2k of stack, 1MiB zeroed with 4k pages */
		constexpr auto iovs = 256;
		iovec iov[iovs];
		for (auto &i : iov) {
			i.iov_base = z;
			i.iov_len = PAGE_SIZE;
		}

		/* write zeros */
		uint64_t off = arg64[0];
		uint64_t len = arg64[1];
		while (len) {
			auto pages = std::min<uint64_t>(len / PAGE_SIZE, iovs);
			auto r = write(iov, pages, off);
			if (r < 0)
				return r;
			assert(!(r & PAGE_MASK));
			off += r;
			len -= r;
		}

		return 0;
	}
	case BLKDISCARDZEROES:
		/* test if BLKDISCARD will zero out data */
		if (!ALIGNED(arg, int))
			return -EINVAL;
		*static_cast<int *>(arg) = v_discard_sets_to_zero();
		return 0;
	case BLKGETSIZE64:
		/* get device size in bytes */
		if (!ALIGNED(arg, uint64_t))
			return -EINVAL;
		*arg64 = size_;
		return 0;
	}

	return v_ioctl(cmd, arg);
}

/*
 * device::transfer - transfer data to/from block device
 */
int
device::transfer(const iovec *iov, size_t count, off_t off, bool write)
{
	std::lock_guard l{mutex_};

	assert(nopens_ > 0);

	if (!count)
		return 0;
	if (off >= size_)
		return 0;

	/* total length truncated to device size */
	const size_t len = std::min(size_ - off, [=]{
		off_t l = 0;
		for (size_t i = 0; i < count; ++i)
			l += iov[i].iov_len;
		return l;
	}());

	std::byte *buf = static_cast<std::byte *>(phys_to_virt(buf_.get()));
	size_t iov_off = 0;
	size_t t = 0;

	/* clean write buffer if reading */
	if (!write)
		sync();

	/* align start of transfer to page boundary */
	if (const size_t align = off & PAGE_MASK; align) {
		if (auto r = fill(off); r < 0)
			return r;
		const auto fix = std::min(PAGE_SIZE - align, len);
		while (t < fix) {
			auto cp = std::min(fix - t, iov->iov_len);
			auto bufloc = buf + align + t;
			if (write) {
				memcpy(bufloc, iov->iov_base, cp);
				dirty_ = true;
			} else memcpy(iov->iov_base, bufloc, cp);
			t += cp;
			if (cp >= iov->iov_len)
				++iov;
			else
				iov_off = cp;
		}
	}

	/* transfer whole pages directly from block device */
	while (len - t >= PAGE_SIZE) {
		auto r = write
		    ? v_write(iov, iov_off, PAGE_TRUNC(len - t), off + t)
		    : v_read(iov, iov_off, PAGE_TRUNC(len - t), off + t);
		if (r < 0)
			return r;
		assert(!(r & PAGE_MASK));
		t += r;
		iov_off += r;
		while (iov_off >= iov->iov_len) {
			iov_off -= iov->iov_len;
			++iov;
		}
	}

	/* final partial page */
	if (t < len) {
		if (auto r = fill(off + t); r < 0)
			return r;
		while (t < len) {
			std::byte *p = static_cast<std::byte *>(iov->iov_base);
			auto cp = std::min(len - t, iov->iov_len - iov_off);
			auto bufloc = buf + ((off + t) & PAGE_MASK);
			if (write) {
				memcpy(bufloc, p + iov_off, cp);
				dirty_ = true;
			} else memcpy(p + iov_off, bufloc, cp);
			t += cp;
			++iov;
			iov_off = 0;
		}
	}

	return t;
}

/*
 * device::fill - fill block buffer
 *
 * 'off' is aligned to the nearest page boundary.
 */
int
device::fill(off_t off)
{
	mutex_.assert_locked();

	off &= ~PAGE_MASK;
	if (off_ == off)
		return 0;
	if (auto r = sync(); r < 0)
		return r;
	iovec iov{phys_to_virt(buf_.get()), PAGE_SIZE};
	if (auto r = v_read(&iov, 0, PAGE_SIZE, off); r != PAGE_SIZE) {
		off_ = std::numeric_limits<off_t>::max();
		return r < 0 ? r : DERR(-EIO);
	}
	off_ = off;
	return 0;
}

/*
 * device::sync - synchronise block buffer with device
 */
int
device::sync()
{
	mutex_.assert_locked();

	if (!dirty_)
		return 0;
	iovec iov{phys_to_virt(buf_.get()), PAGE_SIZE};
	if (auto r = v_write(&iov, 0, PAGE_SIZE, off_); r != PAGE_SIZE)
		return r < 0 ? r : DERR(-EIO);
	dirty_ = false;
	return 0;
}

}
