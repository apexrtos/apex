#include "init.h"
#include "clint.h"

#include <arch/mmio.h>
#include <debug.h>
#include <sections.h>
#include <timer.h>

namespace {

/*
 * SiFive CLINT hardware registers
 */
struct clint {
	uint32_t msip[8];
	uint8_t res1[16352];
	uint32_t mtimecmp[16];
	uint8_t res2[32696];
	uint32_t mtime[2];
};
static_assert(offsetof(clint, mtimecmp) == 0x4000);
static_assert(offsetof(clint, mtime) == 0xbff8);

__fast_bss clint *inst;
__fast_bss uint32_t scale;	    /* scaling from timer to nanoseconds */
__fast_bss uint32_t interval;	    /* tick interval in clocks */
__fast_bss uint64_t prev;	    /* previous mtimecmp value */
__fast_bss std::atomic<uint64_t> monotonic;
constexpr uint32_t tick_ns = 1000000000 / CONFIG_HZ;

void
write_mtimecmp(uint64_t val)
{
	/* Follow the advice in 3.1.10 of the privileged ISA manual to avoid
	 * spurious timer interrupts */
	write32(&inst->mtimecmp[0], 0xffffffff);
	write32(&inst->mtimecmp[1], static_cast<uint32_t>(val >> 32));
	write32(&inst->mtimecmp[0], static_cast<uint32_t>(val));
}

uint64_t
read_mtime()
{
	/* read and handle rollover */
	uint32_t h, l;
	do {
		h = read32(&inst->mtime[1]);
		l = read32(&inst->mtime[0]);
	} while (h != read32(&inst->mtime[1]));

	return h * 0x100000000 + l;
}

}

/*
 * Initialise
 */
void
intc_sifive_clint_init(const intc_sifive_clint_desc *d)
{
	inst = static_cast<clint *>(phys_to_virt(d->base));

	/* scaling factor to nanoseconds */
	scale = 1000000000 / d->clock;
	interval = d->clock / CONFIG_HZ;

	/* TODO: fractional scaling */
	if (scale * d->clock != 1000000000 ||
	    interval * CONFIG_HZ != d->clock)
		panic("clock requires fractional scaling");

	/* set next interrupt time, align interrupts with timebase */
	prev = read_mtime() / interval * interval;
	write_mtimecmp(prev += interval);
}

/*
 * Handle CLINT timer interrupt
 */
__fast_text
void
intc_sifive_clint_timer_irq()
{
	write_mtimecmp(prev += interval);
	timer_tick(monotonic += tick_ns, tick_ns);
}

/*
 * Get monotonic time
 */
uint_fast64_t
timer_monotonic()
{
	if (!inst)
		return 0;
	return read_mtime() * scale;
}

/*
 * Get monotonic time (coarse, fast version), 1/CONFIG_HZ resolution.
 */
uint_fast64_t
timer_monotonic_coarse()
{
	return monotonic;
}
