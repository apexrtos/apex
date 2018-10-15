#include "init.h"

#include <arch.h>
#include <clock.h>
#include <cpu.h>
#include <debug.h>

/*
 * SysTick registers
 */
struct syst {
	union syst_csr {
		struct {
			unsigned ENABLE : 1;
			unsigned TICKINT : 1;
			unsigned CLKSOURCE : 1;
			unsigned : 13;
			unsigned COUNTFLAG : 1;
		};
	} CSR;
	uint32_t RVR;
	uint32_t CVR;
	union syst_calib {
		struct {
			unsigned TENMS : 24;
			unsigned : 6;
			unsigned SKEW : 1;
			unsigned NOREF : 1;
		};
	} CALIB;
};
static_assert(sizeof(struct syst) == 16, "Bad SYST size");
static volatile struct syst *const SYST = (struct syst*)0xe000e010;

/*
 * Initialise
 */
void
arm_armv7m_systick_init(const struct arm_armv7m_systick_desc *d)
{
	/* do not configure twice */
	assert(!SYST->CSR.ENABLE);

	/* set systick timer to interrupt us at CONFIG_HZ */
	SYST->RVR = d->clock / CONFIG_HZ - 1;
	SYST->CVR = 0;

	/* enable timer & interrupts */
	SYST->CSR = (union syst_csr){
		.ENABLE = 1,
		.TICKINT = 1,
		.CLKSOURCE = d->clksource,
	};

	dbg("ARMv7-M SysTick initialised, RVR=%u\n", SYST->RVR);
}

/*
 * Compute how many nanoseconds we are through the current tick
 *
 * Must be called with SysTick interrupt disabled
 */
unsigned long
clock_ns_since_tick(void)
{
	if (!SYST->CSR.ENABLE)
		return 0;

	/* get CVR, making sure that we handle rollovers */
	uint32_t cvr;
	bool tick_pending;
	do {
		tick_pending = SCB->ICSR.PENDSTSET;
		cvr = SYST->CVR;
	} while (tick_pending != SCB->ICSR.PENDSTSET);

	/* convert count to nanoseconds */
	/* REVISIT: fractional multiply instead of 64-bit division? */
	const uint32_t r = SYST->RVR + 1;
	uint32_t ns = cvr ? (r - cvr) * 1000000000ULL / (r * CONFIG_HZ) : 0;
	if (tick_pending)
		ns += 1000000000 / CONFIG_HZ;
	return ns;
}
