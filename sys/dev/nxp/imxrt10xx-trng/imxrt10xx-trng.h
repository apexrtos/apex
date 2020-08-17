#pragma once

#include <device.h>
#include <event.h>
#include <span>
#include <sync.h>

struct nxp_imxrt10xx_trng_desc;
void nxp_imxrt10xx_trng_init(const nxp_imxrt10xx_trng_desc *);

namespace imxrt10xx {

/*
 * True Random Number Generator (TRNG) Controller
 */
class trng {
public:
	static trng *inst();
	ssize_t read(std::span<std::byte>);

private:
	struct regs;
	trng(const nxp_imxrt10xx_trng_desc *d);
	trng(trng &&) = delete;
	trng(const trng &) = delete;
	trng &operator=(trng &&) = delete;
	trng &operator=(const trng&) = delete;

	a::spinlock lock_;
	event wakeup_;
	regs *const r_;
	unsigned index_;

	void isr();
	static int isr_wrapper(int, void *);

	friend void ::nxp_imxrt10xx_trng_init(const nxp_imxrt10xx_trng_desc *);
};

}