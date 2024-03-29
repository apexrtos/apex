#include "init.h"

#include <arch/mmio.h>
#include <atomic>
#include <cpu.h>
#include <debug.h>
#include <irq.h>
#include <sections.h>
#include <timer.h>
#include <v7m/bitfield.h>

namespace {

/*
 * SysTick registers
 */
struct syst {
	union csr {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbit<S, bool, 16> COUNTFLAG;
		bitfield::armbit<S, bool, 2> CLKSOURCE;
		bitfield::armbit<S, bool, 1> TICKINT;
		bitfield::armbit<S, bool, 0> ENABLE;
	} CSR;
	uint32_t RVR;
	uint32_t CVR;
	union calib {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbit<S, bool, 31> NOREF;
		bitfield::armbit<S, bool, 30> SKEW;
		bitfield::armbits<S, unsigned, 23, 0> TENMS;
	} CALIB;
};
static_assert(sizeof(syst) == 16, "Bad SYST size");
static syst *const SYST = (syst*)0xe000e010;

__fast_bss uint64_t scale;
__fast_bss std::atomic<uint64_t> monotonic;
constexpr uint32_t tick_ns = 1000000000 / CONFIG_HZ;

/*
 * Compute how many nanoseconds we are through the current tick
 *
 * Must be called with SysTick interrupt disabled
 */
uint32_t
ns_since_tick()
{
	if (!read32(&SYST->CSR).ENABLE)
		return 0;

	/* get CVR, making sure that we handle rollovers */
	uint32_t cvr = read32(&SYST->CVR);
	bool tick_pending = read32(&SCB->ICSR).PENDSTSET;
	if (tick_pending)
		cvr = read32(&SYST->CVR);

	/* convert count to nanoseconds */
	uint32_t ns = cvr ? ((read32(&SYST->RVR) + 1 - cvr) * scale) >> 32 : 0;
	if (tick_pending)
		ns += tick_ns;
	return ns;
}

}

/*
 * Initialise
 */
void
arm_armv7m_systick_init(const arm_armv7m_systick_desc *d)
{
	/* do not configure twice */
	assert(!read32(&SYST->CSR).ENABLE);

	/* set systick timer to interrupt us at CONFIG_HZ */
	write32(&SYST->RVR, static_cast<uint32_t>(d->clock / CONFIG_HZ - 1));
	write32(&SYST->CVR, 0u);

	/* enable timer & interrupts */
	write32(&SYST->CSR, [&]{
		syst::csr r{};
		r.ENABLE = 1;
		r.TICKINT = 1;
		r.CLKSOURCE = d->clksource;
		return r;
	}());

	/* scaling factor from count to ns * 2^32 */
	scale = 1000000000ull * 0x100000000 / d->clock;

	dbg("ARMv7-M SysTick initialised, RVR=%u\n", read32(&SYST->RVR));
}

/*
 * SysTick exception
 */
extern "C" __fast_text void
exc_SysTick()
{
	timer_tick(monotonic += tick_ns, tick_ns);
}

/*
 * Get monotonic time
 */
uint_fast64_t
timer_monotonic()
{
	const int s = irq_disable();
	const uint_fast64_t r = monotonic + ns_since_tick();
	irq_restore(s);
	return r;
}

/*
 * Get monotonic time (coarse, fast version), 1/CONFIG_HZ resolution.
 */
uint_fast64_t
timer_monotonic_coarse()
{
	return monotonic;
}
