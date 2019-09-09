#ifndef dev_mmc_mmc_block_h
#define dev_mmc_mmc_block_h

/*
 * MMC Block Device
 */

#include <dev/block/device.h>

struct device;

namespace mmc::mmc {

class device;
enum class partition;

class block final : public ::block::device {
public:
	block(mmc::device *, ::device *, partition, off_t size);
	~block() override;

private:
	int v_open() override;
	int v_close() override;
	ssize_t v_read(const iovec *, size_t, size_t, off_t) override;
	ssize_t v_write(const iovec *, size_t, size_t, off_t) override;
	int v_ioctl(unsigned long, void *) override;

	mmc::device *const device_;
	const partition partition_;
};

}

#endif
