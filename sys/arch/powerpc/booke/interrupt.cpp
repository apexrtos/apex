#include <arch/interrupt.h>

#include <booke/locore.h>
#include <cassert>

bool
interrupt_from_userspace()
{
	/* interrupt is from userspace if MSR PR bit is set on outer irq frame */
	assert(interrupt_running());
	irq_frame *f = (irq_frame *)((uintptr_t)mfspr<CPU_DATA>().r->base_irq_stack -
							sizeof(irq_frame));
	MSR msr{.r = f->msr};
	return msr.PR == MSR::ProblemState::Problem;
}

bool
interrupt_running()
{
	const auto msr = mfmsr();
	return mfspr<IRQ_NESTING>().r > 0 || !msr.ME || !msr.DE || !msr.CE;
}
