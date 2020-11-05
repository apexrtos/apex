#include "init.h"

#include <arch.h>
#include <clock.h>
#include <cpu.h>
#include <debug.h>
#include <sections.h>
#include <timer.h>

/*
 * SysTick registers
 */
struct syst {
	union syst_csr {
		struct {
			uint32_t ENABLE : 1;
			uint32_t TICKINT : 1;
			uint32_t CLKSOURCE : 1;
			uint32_t : 13;
			uint32_t COUNTFLAG : 1;
		};
		uint32_t r;
	} CSR;
	uint32_t RVR;
	uint32_t CVR;
	union syst_calib {
		struct {
			uint32_t TENMS : 24;
			uint32_t : 6;
			uint32_t SKEW : 1;
			uint32_t NOREF : 1;
		};
		uint32_t r;
	} CALIB;
};
static_assert(sizeof(struct syst) == 16, "Bad SYST size");
static struct syst *const SYST = (struct syst*)0xe000e010;

/*
 * Initialise
 */
void
arm_armv7m_systick_init(const struct arm_armv7m_systick_desc *d)
{
	/* do not configure twice */
	assert(!read32(&SYST->CSR).ENABLE);

	/* set systick timer to interrupt us at CONFIG_HZ */
	write32(&SYST->RVR, d->clock / CONFIG_HZ - 1);
	write32(&SYST->CVR, 0);

	/* enable timer & interrupts */
	write32(&SYST->CSR, (union syst_csr){
		.ENABLE = 1,
		.TICKINT = 1,
		.CLKSOURCE = d->clksource,
	}.r);

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
	if (!read32(&SYST->CSR).ENABLE)
		return 0;

	/* get CVR, making sure that we handle rollovers */
	uint32_t cvr;
	bool tick_pending;
	do {
		tick_pending = read32(&SCB->ICSR).PENDSTSET;
		cvr = read32(&SYST->CVR);
	} while (tick_pending != read32(&SCB->ICSR).PENDSTSET);

	/* convert count to nanoseconds */
	/* REVISIT: fractional multiply instead of 64-bit division? */
	const uint32_t r = read32(&SYST->RVR) + 1;
	uint32_t ns = cvr ? (r - cvr) * 1000000000ULL / (r * CONFIG_HZ) : 0;
	if (tick_pending)
		ns += 1000000000 / CONFIG_HZ;
	return ns;
}

/*
 * SysTick exception
 */
__fast_text void
exc_SysTick(void)
{
	timer_tick(1);
}
