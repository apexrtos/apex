#pragma once

/*
 * MMC Device Support
 */

#include "device.h"
#include "mmc.h"
#include <list>
#include <memory>

struct device;
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
	int zeroout(partition, off_t, uint64_t);
	int discard(partition, off_t, uint64_t, bool secure);
	bool discard_sets_to_zero();

private:
	static constexpr unsigned rca_ = 1;
	device_type mode_;
	ocr ocr_;
	cid cid_;
	csd csd_;
	ext_csd ext_csd_;
	unsigned sector_size_;
	std::list<block> partitions_;
	::device *rpmb_dev_ = nullptr;

	mode_t v_mode() const override;

	int for_each_eg(off_t, uint64_t, std::function<int(size_t, size_t)>);
	int switch_partition(partition);
	int add_partitions();
};

}
