#include "mmc_block.h"

#include "mmc_device.h"

namespace mmc::mmc {

/*
 * block::block
 */
block::block(mmc::device *md, ::device *d, partition p, off_t size)
: block::device{d, size}
, device_{md}
, partition_{p}
{ }

/*
 * block::~block
 */
block::~block()
{ }

/*
 * block::v_open
 */
int
block::v_open()
{
	return 0;
}

/*
 * block::v_close
 */
int
block::v_close()
{
	return 0;
}

/*
 * block::v_read
 */
ssize_t
block::v_read(const iovec *iov, size_t iov_off, size_t len, off_t off)
{
	return device_->read(partition_, iov, iov_off, len, off);
}

/*
 * block::v_write
 */
ssize_t
block::v_write(const iovec *iov, size_t iov_off, size_t len, off_t off)
{
	return device_->write(partition_, iov, iov_off, len, off);
}

/*
 * block::v_ioctl
 */
int
block::v_ioctl(unsigned long cmd, void *arg)
{
	return device_->ioctl(partition_, cmd, arg);
}

/*
 * block::v_zeroout
 */
int
block::v_zeroout(off_t off, uint64_t len)
{
	return device_->zeroout(partition_, off, len);
}

/*
 * block::v_discard
 */
int
block::v_discard(off_t off, uint64_t len, bool secure)
{
	return device_->discard(partition_, off, len, secure);
}

/*
 * block::v_discard_sets_to_zero
 */
bool
block::v_discard_sets_to_zero()
{
	return device_->discard_sets_to_zero();
}

}
