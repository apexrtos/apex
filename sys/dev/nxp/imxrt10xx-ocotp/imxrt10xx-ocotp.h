#pragma once

#include <device.h>
#include <sync.h>

struct nxp_imxrt10xx_ocotp_desc;
extern "C" void nxp_imxrt10xx_ocotp_init(const nxp_imxrt10xx_ocotp_desc *);

namespace imxrt10xx {

/*
 * On-Chip One Time Programmable (OCOTP) Controller
 */
class ocotp {
public:
	static ocotp *inst();

	/* interface to filesystem */
	ssize_t read(file *, void *, size_t, off_t);
	ssize_t write(file *, void *, size_t, off_t);

private:
	struct regs;
	ocotp(const nxp_imxrt10xx_ocotp_desc *d);
	ocotp(const ocotp&) = delete;
	ocotp& operator=(const ocotp&) = delete;

	a::mutex mutex_;
	regs *const r_;

	void reload_shadow();
	void wait_busy();
	bool check_and_clear_error();
	friend void ::nxp_imxrt10xx_ocotp_init(const nxp_imxrt10xx_ocotp_desc *);
};

}
