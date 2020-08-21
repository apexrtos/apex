#pragma once

#include <device.h>
#include <span>
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

	ssize_t read(std::span<std::byte>, off_t);
	ssize_t write(std::span<const std::byte>, off_t);

private:
	struct regs;
	ocotp(const nxp_imxrt10xx_ocotp_desc *d);
	ocotp(ocotp &&) = delete;
	ocotp(const ocotp &) = delete;
	ocotp &operator=(ocotp &&) = delete;
	ocotp &operator=(const ocotp&) = delete;

	a::mutex mutex_;
	regs *const r_;

	void reload_shadow();
	void wait_busy();
	bool check_and_clear_error();
	friend void ::nxp_imxrt10xx_ocotp_init(const nxp_imxrt10xx_ocotp_desc *);
};

}
