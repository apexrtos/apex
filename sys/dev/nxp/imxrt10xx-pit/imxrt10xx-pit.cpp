#include "imxrt10xx-pit.h"

#include "init.h"
#include <arch/mmio.h>
#include <debug.h>
#include <irq.h>
#include <kernel.h>
#include <new>

#define trace(...)

namespace {

std::aligned_storage_t<sizeof(imxrt10xx::pit), alignof(imxrt10xx::pit)> mem;
using namespace std::chrono_literals;

}

namespace imxrt10xx {

/*
 * Registers
 */
struct pit::regs {
	union mcr {
		struct {
			uint32_t FRZ : 1;
			uint32_t MDIS : 1;
			uint32_t : 30;
		};
		uint32_t r;
	} MCR;
	uint32_t reserved[55];
	uint32_t LTMR64H;
	uint32_t LTMR64L;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	struct {
		uint32_t LDVAL;
		uint32_t CVAL;

		union tctrl {
			struct {
				uint32_t TEN : 1;
				uint32_t TIE : 1;
				uint32_t CHN : 1;
				uint32_t : 29;
			};
			uint32_t r;
		} TCTRL;

		union tflg {
			struct {
				uint32_t TIF : 1;
				uint32_t : 31;
			};
			uint32_t r;
		} TFLG;
	} channel[pit::channels];
};

/*
 * Periodic Interrupt Timer Module
 */

pit::pit(const nxp_imxrt10xx_pit_desc *d)
: r_{reinterpret_cast<regs*>(d->base)}
, clock_{d->clock}
{
	static_assert(sizeof(regs) == 0x140, "");
	static_assert(BYTE_ORDER == LITTLE_ENDIAN, "");

	write32(&r_->MCR, [&]{
		decltype(r_->MCR) v{};
		v.FRZ = 1;	/* Timers are stopped in Debug mode */
		v.MDIS = 0;	/* Module enabled */
		return v.r;
	}());

	::irq_attach(d->irq, d->ipl, 0, isr_wrapper, nullptr, this);
}

pit *
pit::inst()
{
	return reinterpret_cast<pit *>(&mem);
}

int
pit::start(unsigned ch, std::chrono::nanoseconds t)
{
	trace("PIT(%p) channel[%d] Start - Period: %lldns\n", r_,
		ch, t.count());

	if (ch >= channels)
		return DERR(-EINVAL);

	/* sanity check requested period */
	if (t > std::chrono::nanoseconds{UINT32_MAX * 1s + 1s} / clock_)
		return DERR(-ERANGE);

	const auto v = div_closest(clock_ * t.count(), std::nano::den);
	if (v <= 0)
		return DERR(-ERANGE);

	std::lock_guard l{lock_};
	/* write the period to the load register */
	write32(&r_->channel[ch].LDVAL, static_cast<uint32_t>(v - 1));
	/* enable the channel */
	write32(&r_->channel[ch].TCTRL, [&]{
		auto v = read32(&r_->channel[ch].TCTRL);
		v.TEN = 1;
		return v.r;
	}());

	return 0;
}

void
pit::stop(unsigned ch)
{
	std::lock_guard l{lock_};
	assert(ch < channels);
	trace("PIT(%p) CH[%d] Stop\n", r_, ch);
	write32(&r_->channel[ch].TCTRL, [&]{
		auto v = read32(&r_->channel[ch].TCTRL);
		v.TEN = 0;
		return v.r;
	}());
}

std::chrono::nanoseconds
pit::get(unsigned ch)
{
	assert(ch < channels);
	auto v = read32(&r_->channel[ch].CVAL);
	return std::chrono::nanoseconds{(v + 1) * 1s} / clock_;
}

int
pit::irq_attach(unsigned ch, isr_fn fn)
{
	std::lock_guard l{lock_};

	trace("PIT(%p) channel[%d] Attach IRQ\n", r_, ch);
	if (ch >= channels)
		return DERR(-EINVAL);
	if (irq_table_[ch])
		return DERR(-EBUSY);

	irq_table_[ch] = fn;

	write32(&r_->channel[ch].TCTRL, [&]{
		auto v = read32(&r_->channel[ch].TCTRL);
		v.TIE = 1;
		return v.r;
	}());

	return 0;
}

void
pit::irq_detach(unsigned ch)
{
	std::lock_guard l{lock_};
	assert(ch < channels);

	trace("PIT(%p) channel[%d] Detach IRQ\n", r_, ch);
	irq_table_[ch] = nullptr;

	write32(&r_->channel[ch].TCTRL, [&]{
		auto v = read32(&r_->channel[ch].TCTRL);
		v.TIE = 0;
		return v.r;
	}());
}

void
pit::isr()
{
	/* scan through all channels to see which raised the interrupt */
	for (auto ch = 0u; ch < channels; ++ch) {
		const auto v = read32(&r_->channel[ch].TFLG);

		/* this channel didnt raise the interrupt - continue */
		if (!v.TIF)
			continue;

		/* write 1 to the flag to clear the interrupt */
		write32(&r_->channel[ch].TFLG, v.r);
		std::unique_lock l{lock_};
		auto e = irq_table_[ch];
		l.unlock();
		/* execute user attached function - if any */
		if (e)
			e(ch);
	}
}

int
pit::isr_wrapper(int vector, void *data)
{
	auto p = static_cast<pit *>(data);
	p->isr();
	return INT_DONE;
}

}

/*
 * nxp_imxrt10xx_pit_init
 */
void
nxp_imxrt10xx_pit_init(const nxp_imxrt10xx_pit_desc *d)
{
	notice("PIT(%p) Init\n", (void*)d->base);
	new(&mem) imxrt10xx::pit{d};
}
