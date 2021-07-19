#include "init.h"

#include <arch/mmio.h>
#include <cassert>
#include <debug.h>
#include <irq.h>
#include <sections.h>
#include <timer.h>

namespace {

struct regs {
	union cr {
		struct {
			uint32_t EN : 1;
			uint32_t ENMOD : 1;
			uint32_t DBGEN : 1;
			uint32_t WAITEN : 1;
			uint32_t DOZEEN : 1;
			uint32_t STOPEN : 1;
			uint32_t CLKSRC : 3;
			uint32_t FRR : 1;
			uint32_t EN_24M : 1;
			uint32_t : 4;
			uint32_t SWR : 1;
			uint32_t IM1 : 2;
			uint32_t IM2 : 2;
			uint32_t OM1 : 3;
			uint32_t OM2 : 3;
			uint32_t OM3 : 3;
			uint32_t FO1 : 1;
			uint32_t FO2 : 1;
			uint32_t FO3 : 1;
		};
		uint32_t r;
	} CR;
	union pr {
		struct {
			uint32_t PRESCALER : 12;
			uint32_t PRESCALER24M : 4;
			uint32_t : 16;
		};
		uint32_t r;
	} PR;
	union sr {
		struct {
			uint32_t OF1 : 1;
			uint32_t OF2 : 1;
			uint32_t OF3 : 1;
			uint32_t IF1 : 1;
			uint32_t IF2 : 1;
			uint32_t ROV : 1;
			uint32_t : 26;
		};
		uint32_t r;
	} SR;
	union ir {
		struct {
			uint32_t OF1IE : 1;
			uint32_t OF2IE : 1;
			uint32_t OF3IE : 1;
			uint32_t IF1IE : 1;
			uint32_t IF2IE : 1;
			uint32_t ROVIE : 1;
			uint32_t : 26;
		};
		uint32_t r;
	} IR;
	uint32_t OCR[3];
	uint32_t ICR[2];
	uint32_t CNT;
};
static_assert(sizeof(regs) == 0x28);
static_assert(BYTE_ORDER == LITTLE_ENDIAN);

__fast_bss regs *gpt;
__fast_bss uint64_t scale;

__fast_text int
fsl_gpt_systick_isr(int, void *)
{
	/* acknowledge interrupt */
	write32(&gpt->SR, {.r = 0xffffffff});

	timer_tick(1);
	return INT_DONE;
}

}

void
fsl_gpt_systick_init(const fsl_gpt_systick_desc *d)
{
	/* do not configure twice */
	assert(!gpt);

	gpt = reinterpret_cast<regs *>(d->base);

	/* configure timer to interrupt us at CONFIG_HZ */
	regs::cr cr{{
		.WAITEN = 1,
		.DOZEEN = 1,
		.STOPEN = 1,
		.CLKSRC = static_cast<unsigned>(d->clksrc),
		.EN_24M = d->clksrc == fsl::gpt::clock::ipg_24M,
	}};
	write32(&gpt->CR, cr);
	write32(&gpt->PR, {{
		.PRESCALER = d->prescaler - 1,
		.PRESCALER24M = d->prescaler_24M - 1,
	}});
	write32(&gpt->OCR[0], static_cast<uint32_t>(d->clock / d->prescaler / CONFIG_HZ - 1));
	write32(&gpt->IR, {{.OF1IE = 1}});

	if (!irq_attach(d->irq, d->ipl, 0, fsl_gpt_systick_isr, 0, 0))
		panic("irq_attach");

	/* start timer */
	cr.EN = 1;
	write32(&gpt->CR, cr);

	/* scaling factor from count to ns * 2^32 */
	scale = 1000000000ull * 0x100000000 / (d->clock / d->prescaler);

	dbg("GPT System Timer initialised, OCR1=%u\n", read32(&gpt->OCR[0]));
}

/*
 * Compute how many nanoseconds we are through the current tick
 *
 * Must be called with gpt interrupt disabled
 */
unsigned long
clock_ns_since_tick()
{
	if (!gpt)
		return 0;

	/* get CNT, making sure that we handle rollovers */
	uint32_t cnt = read32(&gpt->CNT);
	bool tick_pending = read32(&gpt->SR).OF1;
	if (tick_pending)
		cnt = read32(&gpt->CNT);

	/* convert count to nanoseconds */
	uint32_t ns = (cnt * scale) >> 32;
	if (tick_pending)
		ns += 1000000000 / CONFIG_HZ;
	return ns;
}
