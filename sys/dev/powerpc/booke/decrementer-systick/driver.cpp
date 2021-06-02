#include "init.h"

#include <arch/interrupt.h>
#include <cassert>
#include <clock.h>
#include <cpu.h>
#include <debug.h>
#include <sections.h>
#include <timer.h>

__fast_bss uint64_t scale;
__fast_bss uint32_t prev_tbl;
__fast_bss uint32_t period;

/*
 * Initialise
 */
void
powerpc_booke_decrementer_systick_init(
	const struct powerpc_booke_decrementer_systick_desc *d)
{
	/* configure decrementer to interrupt us at CONFIG_HZ */
	period = d->clock / CONFIG_HZ;
	prev_tbl = mfspr<TBL>().r;
	mtspr<DEC>({period});
	mtspr<DECAR>({period});

	/* clear pending interrupt */
	mtspr<TSR>({.DIS = true});

	/* enable decrementer interrupt and auto reload */
	mtspr<TCR>([] {
		auto tcr = mfspr<TCR>();
		tcr.DIE = true;
		tcr.ARE = true;
		return tcr;
	}());

	/* scaling factor from timebase to ns * 2^32 */
	scale = 1000000000ull * 0x100000000 / d->clock;

	dbg("PowerPC BookE Decrementer initialised, DECAR=%u\n", period);
}

/*
 * Compute how many nanoseconds we are through the current tick
 *
 * Must be called with decrementer exception disabled
 */
unsigned long
clock_ns_since_tick()
{
	return ((mfspr<TBL>().r - prev_tbl) * scale) >> 32;
}

/*
 * Decrementer exception
 */
extern "C" __fast_text void
exc_Decrementer()
{
	/* acknowledge exception & allow interrupt nesting */
	mtspr<TSR>({.DIS = true});
	interrupt_enable();

	/* work out how many ticks passed since last interrupt */
	auto elapsed = mfspr<TBL>().r - prev_tbl;
	auto ticks = elapsed / period;
	prev_tbl += ticks * period;
	timer_tick(ticks);
}
