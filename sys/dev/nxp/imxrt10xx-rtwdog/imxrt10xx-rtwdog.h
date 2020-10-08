#pragma once

#include <atomic>
#include <span>
#include <sync.h>

struct nxp_imxrt10xx_rtwdog_desc;
void nxp_imxrt10xx_rtwdog_init(const nxp_imxrt10xx_rtwdog_desc *);

namespace imxrt10xx {

/*
 * Driver for RT Watchdog (RTWDOG) on IMXRT10xx processors
 */
class rtwdog {
public:
	enum class clock {
		bus,
		lpo,
		intclk,
		erclk,
	};

	static rtwdog *inst();
	int open();
	int close();
	int ioctl(unsigned long, void *);
	ssize_t write(std::span<const std::byte>, off_t);

private:
	struct regs;
	rtwdog(const nxp_imxrt10xx_rtwdog_desc *);
	rtwdog(rtwdog &&) = delete;
	rtwdog(const rtwdog &) = delete;
	rtwdog &operator=(rtwdog &&) = delete;
	rtwdog &operator=(const rtwdog &) = delete;

	a::spinlock_irq lock_;
	regs *const r_;
	const unsigned long clock_;
	std::atomic<bool> open_;
	bool expect_close_;

	void configure(clock, bool);
	void enable(bool);
	int set_timeout(unsigned);
	int get_timeout();
	int get_timeleft();
	void refresh();

	friend void ::nxp_imxrt10xx_rtwdog_init(const nxp_imxrt10xx_rtwdog_desc *);
};

}
