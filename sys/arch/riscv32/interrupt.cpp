#include <arch/interrupt.h>

#include <cassert>
#include <compiler.h>
#include <cpu.h>
#include <intrinsics.h>
#include <thread.h>

void
interrupt_enable()
{
	compiler_barrier();
#ifdef CONFIG_S_MODE
	csrs(sstatus{.SIE = true});
#else
	csrs(mstatus{.MIE = true});
#endif
}

void
interrupt_disable()
{
#ifdef CONFIG_S_MODE
	csrc(sstatus{.SIE = true});
#else
	csrc(mstatus{.MIE = true});
#endif
	compiler_barrier();
}

void
interrupt_save_disable(int *l)
{
#ifdef CONFIG_S_MODE
	*l = csrrc(sstatus{.SIE = true}).SIE.raw();
#else
	*l = csrrc(mstatus{.MIE = true}).MIE.raw();
#endif
	compiler_barrier();
}

void
interrupt_restore(int l)
{
#ifdef CONFIG_S_MODE
	csrs(sstatus{.r = static_cast<uint32_t>(l)});
#else
	csrs(mstatus{.r = static_cast<uint32_t>(l)});
#endif
}

bool
interrupt_enabled()
{
#ifdef CONFIG_S_MODE
	return csrr<sstatus>().SIE;
#else
	return csrr<mstatus>().MIE;
#endif
}

bool
interrupt_from_userspace()
{
	assert(interrupt_running());

#ifdef CONFIG_S_MODE
	return csrr<sstatus>().SPP == 0;
#else
	return csrr<mstatus>().MPP == 0;
#endif
}

bool
interrupt_running()
{
	return thread_cur()->ctx.irq_nesting > 0;
}
