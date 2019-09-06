#ifndef dev_block_device_h
#define dev_block_device_h

/*
 * Generic Block Device
 */

#include <memory>
#include <page.h>
#include <sync.h>

struct device;
struct iovec;

namespace block {

class device {
public:
	device(::device *, off_t size);
	virtual ~device();

	int open();
	int close();
	ssize_t read(const iovec *, size_t, off_t);
	ssize_t write(const iovec *, size_t, off_t);
	int ioctl(unsigned long, void *);

private:
	virtual int v_open() = 0;
	virtual int v_close() = 0;
	virtual ssize_t v_read(const iovec *, size_t, size_t, off_t) = 0;
	virtual ssize_t v_write(const iovec *, size_t, size_t, off_t) = 0;
	virtual int v_ioctl(unsigned long, void *) = 0;

	ssize_t transfer(const iovec *, size_t, off_t, bool);
	int fill(off_t);
	int sync();

	a::mutex mutex_;
	::device *dev_;
	size_t nopens_;
	off_t size_;
	std::unique_ptr<phys> buf_;
	off_t off_;
	bool dirty_;
};

}

#endif
