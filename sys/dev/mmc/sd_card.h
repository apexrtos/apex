#pragma once

/*
 * SD Card Support
 */

#include "device.h"
#include "sd.h"

namespace mmc::sd {

class block;

class card final : public device {
public:
	card(host *);
	~card() override;

	int init();

	ssize_t read(const iovec *, size_t, size_t, off_t);
	ssize_t write(const iovec *, size_t, size_t, off_t);
	int ioctl(unsigned long, void *);
	int zeroout(off_t, uint64_t);
	int discard(off_t, uint64_t, bool secure);
	bool discard_sets_to_zero();

private:
	unsigned rca_;
	access_mode mode_;
	ocr ocr_;
	cid cid_;
	csd csd_;
	scr scr_;
	status status_;
	unsigned sector_size_;
	std::unique_ptr<block> block_;

	mode_t v_mode() const override;

	int for_each_au(off_t, uint64_t, std::function<int(size_t, size_t)>);
	uint64_t size() const;
};

}
