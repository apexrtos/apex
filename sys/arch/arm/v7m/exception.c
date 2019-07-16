#include <arch.h>

#include <context.h>
#include <cpu.h>
#include <debug.h>
#include <irq.h>
#include <sections.h>
#include <sig.h>
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
	if (cfsr.MMFSR.MMARVALID)
		mpu_fault((void *)read32(&SCB->MMFAR), 0);
	else if (cfsr.MMFSR.IACCVIOL)
		mpu_fault((void *)e->ra, 4);
	else if (cfsr.MMFSR.MSTKERR)
		sig_thread(thread_cur(), SIGSEGV); /* stack overflow */
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

	dbg("BusFault");
	sig_thread(thread_cur(), SIGBUS);

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
}

