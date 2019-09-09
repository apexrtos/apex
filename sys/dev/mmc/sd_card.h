#ifndef dev_mmc_sd_card_h
#define dev_mmc_sd_card_h

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

private:
	unsigned rca_;
	access_mode mode_;
	ocr ocr_;
	cid cid_;
	csd csd_;
	scr scr_;
	unsigned sector_size_;
	std::unique_ptr<block> block_;

	mode_t v_mode() const override;
};

}

#endif
