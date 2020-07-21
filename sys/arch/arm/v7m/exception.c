#include <arch.h>

#include "exception_frame.h"
#include <cpu.h>
#include <debug.h>
#include <irq.h>
#include <proc.h>
#include <sch.h>
#include <sections.h>
#include <sig.h>
#include <task.h>
#include <thread.h>

/*
 * dump_exception
 */
static void
dump_exception(struct exception_frame_basic *e, bool handler_mode, int exc)
{
	emergency("%s mode exception %d, thread %p\n",
	    handler_mode ? "Handler" : "Thread", exc, thread_cur());
	emergency(" r0 %08x r1 %08x r2 %08x   r3 %08x\n",
	    e->r0, e->r1, e->r2, e->r3);
	emergency("r12 %08x lr %08x ra %08x xpsr %08x\n",
	    e->r12, e->lr, e->ra, e->xpsr);
}

/*
 * derived_exception - handle a derived exception
 *
 * A derived exception occurs when an exception entry sequence causes a fault.
 *
 * If this happens then we have unrecoverably lost the volatile registers
 * required to deliver a signal or to return to the interrupted context. There
 * is no option other than to terminate the process.
 */
static void
derived_exception(int sig)
{
	/* If the failure happened when attempting to enter SVCall (most likely
	 * due to stack overflow) we need to clear the pending SVCall exception
	 * otherwise it will still run before we enter PendSV to switch away
	 * from this thread. */
	union scb_shcsr shcsr = read32(&SCB->SHCSR);
	shcsr.SVCALLPENDED = 0;
	write32(&SCB->SHCSR, shcsr.r);

	/* kill the process */
	proc_exit(task_cur(), 0, sig);

	/* switch away from this dying thread soon.. */
	sch_testexit();

	/* NOTE: trying to step through the exception return after a derived
	 * exception in a debugger can be difficult. We rely on tail-chaning to
	 * PendSV to avoid an exception during unstacking of the exception
	 * frame. Single step debugging can stop the tail-chain from happening
	 * and result in another fault.*/
}

/*
 * exc_Unhandled
 */
void
exc_Unhandled(struct exception_frame_basic *e, bool handler_mode, int exc)
{
	dump_exception(e, handler_mode, exc);
	panic("Unhandled");
}

/*
 * exc_MemManage
 */
#if defined(CONFIG_MPU)
__fast_text
#endif
void
exc_MemManage(struct exception_frame_basic *e, bool handler_mode, int exc)
{
#if !defined(CONFIG_MPU)
	dump_exception(e, handler_mode, exc);
	panic("MemManage");
#else
	if (handler_mode || !interrupt_from_userspace()) {
		/* kernel faults are always fatal */
		dump_exception(e, handler_mode, exc);
		panic("MemManage: kernel fault");
	}

	/* try to handle fault */
	const union scb_cfsr cfsr = read32(&SCB->CFSR);
	if (cfsr.MMFSR.MSTKERR)
		derived_exception(SIGSEGV);
	else if (cfsr.MMFSR.MMARVALID)
		mpu_fault((void *)read32(&SCB->MMFAR), 0);
	else if (cfsr.MMFSR.IACCVIOL)
		mpu_fault((void *)e->ra, 4);
	else {
		dump_exception(e, handler_mode, exc);
		panic("MemManage: unexpected fault");
	}

	/* clear fault */
	write8(&SCB->CFSR.MMFSR, -1);
#endif
}

/*
 * exc_BusFault
 */
void
exc_BusFault(struct exception_frame_basic *e, bool handler_mode, int exc)
{
	dump_exception(e, handler_mode, exc);

	/* kernel faults are always fatal */
	if (handler_mode || !interrupt_from_userspace())
		panic("BusFault");

	const union scb_cfsr cfsr = read32(&SCB->CFSR);
	if (cfsr.BFSR.STKERR)
		derived_exception(SIGBUS);
	else {
		dbg("BusFault");
		sig_thread(thread_cur(), SIGBUS);
	}

	/* clear fault */
	write8(&SCB->CFSR.BFSR, -1);
}

/*
 * exc_UsageFault
 */
void
exc_UsageFault(struct exception_frame_basic *e, bool handler_mode, int exc)
{
	const union scb_cfsr cfsr = read32(&SCB->CFSR);
	const char *what = "Usage Fault\n";
	int sig = SIGILL;

	if (cfsr.UFSR.DIVBYZERO) {
		what = "Divide by Zero\n";
		sig = SIGFPE;
	} if (cfsr.UFSR.UNDEFINSTR)
		what = "Undefined Instruction\n";
	else if (cfsr.UFSR.INVSTATE)
		what = "Invalid State\n";
	else if (cfsr.UFSR.INVPC) {
		dump_exception(e, handler_mode, exc);
		panic("Invalid PC");		/* Always fatal */
	} else if (cfsr.UFSR.UNALIGNED)
		what = "Invalid Unaligned Access\n";
	else if (cfsr.UFSR.NOCP)
		what = "Invalid Coprocessor Access\n";

	dump_exception(e, handler_mode, exc);
	if (handler_mode || !interrupt_from_userspace())
		panic(what);
	dbg(what);
	sig_thread(thread_cur(), sig);

	/* clear fault */
	write16(&SCB->CFSR.UFSR, -1);
}

/*
 * exc_SysTick
 */
__fast_text void
exc_SysTick(void)
{
	timer_tick(1);
}

/*
 * exc_NVIC
 */
__fast_text void
exc_NVIC(void)
{
	int ipsr;
	asm("mrs %0, ipsr" : "=r" (ipsr));
	irq_handler(ipsr - 16);

	/* guarantee that writes to peripheral registers complete before
	 * returning from interrupt - this is so that an interrupt can't
	 * spuriously re-trigger if the CPU returns from interrupt before the
	 * write to clear a peripheral's interrupt flag register completes */
	write_memory_barrier();
}

