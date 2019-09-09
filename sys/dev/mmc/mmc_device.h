#ifndef dev_mmc_mmc_device_h
#define dev_mmc_mmc_device_h

/*
 * MMC Device Support
 */

#include "device.h"
#include "mmc.h"
#include <list>
#include <memory>

struct iovec;

namespace mmc::mmc {

class block;

class device final : public ::mmc::device {
public:
	device(host *);
	~device() override;

	int init();

	ssize_t read(partition, const iovec *, size_t, size_t, off_t);
	ssize_t write(partition, const iovec *, size_t, size_t, off_t);
	int ioctl(partition, unsigned long, void *);

private:
	static constexpr unsigned rca_ = 1;
	device_type mode_;
	ocr ocr_;
	cid cid_;
	csd csd_;
	ext_csd ext_csd_;
	unsigned sector_size_;
	std::list<block> partitions_;

	mode_t v_mode() const override;

	int switch_partition(partition);
	int add_partitions();
};

}

#endif
