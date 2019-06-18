#include <arch.h>

#include <asm.h>
#include <cpu.h>
#include <debug.h>
#include <errno.h>
#include <irq.h>
#include <sch.h>
#include <sections.h>
#include <sig.h>
#include <syscall_table.h>
#include <task.h>
#include <thread.h>

// #define TRACE_SYSCALLS

/*
 * offsets into structs for use in assembly below
 */
#define THREAD_TLS 168
#define THREAD_KREGS 172
#define EFRAME_R0 0
#define EFRAME_R1 4
#define EFRAME_R2 8
#define EFRAME_R3 12
#define EFRAME_R12 16
#define EFRAME_LR 20
#define EFRAME_RA 24
#define KREGS_MSP 0
#define KREGS_PSP 4
#define KREGS_SHCSR 8
#ifndef CONFIG_FPU
#define EFRAME_SIZE 32
#else
#define EFRAME_SIZE 104
#endif
static_assert(offsetof(struct thread, ctx.tls) == THREAD_TLS, "");
static_assert(offsetof(struct thread, ctx.kregs) == THREAD_KREGS, "");
static_assert(offsetof(struct exception_frame, r0) == EFRAME_R0, "");
static_assert(offsetof(struct exception_frame, r1) == EFRAME_R1, "");
static_assert(offsetof(struct exception_frame, r2) == EFRAME_R2, "");
static_assert(offsetof(struct exception_frame, r3) == EFRAME_R3, "");
static_assert(offsetof(struct exception_frame, r12) == EFRAME_R12, "");
static_assert(offsetof(struct exception_frame, lr) == EFRAME_LR, "");
static_assert(offsetof(struct exception_frame, ra) == EFRAME_RA, "");
static_assert(offsetof(struct kern_regs, msp) == KREGS_MSP, "");
static_assert(offsetof(struct kern_regs, psp) == KREGS_PSP, "");
static_assert(offsetof(struct kern_regs, shcsr) == KREGS_SHCSR, "");
static_assert(sizeof(struct exception_frame) == EFRAME_SIZE, "");

/*
 * exception entry & exit
 *
 * Exceptions from userspace threads return through PendSV. This guarantees
 * that there are no nested interrupts on return to userspace.
 *
 * Exceptions from kernel threads do an immediate sch_unlock().
 *
 * SVCall returns directly to userspace and may not reschedule.
 *
 * UsageFault sometimes needs to emulate an instruction. In this case it
 * fiddles with some registers and returns immediately.
 *
 * Note: userspace threads run at execution priority 256
 *	 kernel threads run at execution priority 255/254 (SVCall/PendSV)
 *	 other exceptions run at priority < 254
 */
#define EXCEPTION_ENTRY() \
	do { \
		if (!((int)__builtin_return_address(0) & EXC_SPSEL)) \
			sch_lock(); \
		else if (!sch_locked()) { \
			write32(&SCB->ICSR, (union scb_icsr){.PENDSVSET = 1}.r); \
			sch_lock(); \
		} \
	} while (0)

#define EXCEPTION_EXIT() \
	do { \
		if (!((int)__builtin_return_address(0) & EXC_SPSEL)) \
			sch_unlock(); \
	} while (0)

/*
 * exc_PendSV
 *
 * Runs (via tail-chain) after an exception from userspace finishes
 */
__fast_text __attribute__((naked)) void
exc_PendSV(void)
{
	asm(
		"push {r4, r5, r6, r7}\n"    /* struct syscall_frame */
		"bl exc_PendSV_c\n"
		"mov r0, -"S(EPENDSV_RETURN)"\n"
		"bl sig_deliver\n"
		"add sp, 16\n"
		"mov r0, "S(EXC_RETURN_USER)"\n"
		"bx r0\n"
	);
}

__fast_text __attribute__((used)) void
exc_PendSV_c(void)
{
	assert(thread_cur()->locks == 1);
	sch_unlock();
}

/*
 * arch_schedule
 */
void
arch_schedule(void)
{
	sch_switch();
}

/*
 * exc_NMI/context_switch
 *
 * Context switch
 *
 * There are two cases for context switch:
 * 1. thread going to sleep voluntarily
 * 2. preemption (via interrupt)
 * If we are going to sleep we switch via NMI to generate an exception frame
 * which can later be unwound to continue execution. This also puts the CPU
 * into a state where we can switch SHCSR. Note that we only switch the PendSV
 * and SVCall bits as these are the two modes a kernel thread can run in.
 */
asm(
".text\n"
".thumb_func\n"
#if defined(CONFIG_FPU)
".fpu vfp\n"
#endif
".global context_switch\n"
"context_switch:\n"
	/* Check active vector */
	"mov ip, "S(A_SCS)"\n"
	"ldr r2, [ip, "S(A_ICSR)"-"S(A_SCS)"]\n"
	"ubfx r2, r2, 0, 9\n"
	"cmp r2, 11\n"
	"beq 1f\n"	/* NMI if 11 (SVCall) */
	"cmp r2, 14\n"
	"beq 1f\n"	/* NMI if 14 (PendSV) */

	/* switch registers */
".thumb_func\n"
".global exc_NMI\n"
"exc_NMI:\n"
#if defined(CONFIG_FPU)
#if defined(CONFIG_MPU)
	/*
	 * Clear USER bit in FPCCR. This makes the core perform its lazy fpu
	 * stacking using privileged accesses.
	 *
	 * We need to do this because we may have switched to a new task and
	 * the mappings allowing unprivileged access to the previous userspace
	 * stack may no longer be present.
	 */
	"ldr r2, [ip, "S(A_FPCCR)"-"S(A_SCS)"]\n"
	"bic r2, 0x2\n"
	"str r2, [ip, "S(A_FPCCR)"-"S(A_SCS)"]\n"
#endif
	"vpush {s16-s31}\n"
#endif
	"push {r4, r5, r6, r7, r8, r9, r10, r11, lr}\n"
	"ldr r2, [ip, "S(A_SHCSR)"-"S(A_SCS)"]\n"
	"str sp, [r0, "S(THREAD_KREGS)"+"S(KREGS_MSP)"]\n"
	"mrs r3, psp\n"
	"str r3, [r0, "S(THREAD_KREGS)"+"S(KREGS_PSP)"]\n"
	"str r2, [r0, "S(THREAD_KREGS)"+"S(KREGS_SHCSR)"]\n"
	"ldr sp, [r1, "S(THREAD_KREGS)"+"S(KREGS_MSP)"]\n"
	"ldr r3, [r1, "S(THREAD_KREGS)"+"S(KREGS_PSP)"]\n"
	"msr psp, r3\n"
	"ldr r3, [r1, "S(THREAD_KREGS)"+"S(KREGS_SHCSR)"]\n"
	"bic r2, 0x480\n"   /* switch PendSV and SVCall active bits */
	"and r3, 0x480\n"
	"orr r2, r3\n"
	"str r2, [ip, "S(A_SHCSR)"-"S(A_SCS)"]\n"
	"pop {r4, r5, r6, r7, r8, r9, r10, r11, lr}\n"
#if defined(CONFIG_FPU)
	"vpop {s16-s31}\n"
#endif
#if defined(CONFIG_MPU)
	"b mpu_thread_switch\n"
#else
	"bx lr\n"
#endif

	/* Trigger NMI */
	"1: mov r3, 0x80000000\n"
	"str r3, [ip, "S(A_ICSR)"-"S(A_SCS)"]\n"
	"dsb\n"
	"bx lr\n"
);

/*
 * dump_exception
 */
static void
dump_exception(bool from_user, struct exception_frame *e)
{
	emergency("%s mode exception, thread %p\n",
	    from_user ? "User" : "Kernel", thread_cur());
	emergency(" r0 %08x r1 %08x r2 %08x   r3 %08x\n", e->r0, e->r1, e->r2, e->r3);
	emergency("r12 %08x lr %08x ra %08x xpsr %08x\n", e->r12, e->lr, e->ra, e->xpsr);
}

/*
 * exc_HardFault
 */
void
exc_HardFault(void)
{
	const bool from_user = (int)__builtin_return_address(0) & EXC_SPSEL;
	struct exception_frame *e;
	if (from_user)
		asm("mrs %0, psp" : "=r" (e));
	else
		e = __builtin_frame_address(0);
	dump_exception(from_user, e);
	panic("HardFault");
}

/*
 * exc_MemManage
 */
#if defined(CONFIG_MPU)
__fast_text
#endif
void
exc_MemManage(void)
{
	EXCEPTION_ENTRY();

	const bool from_user = (int)__builtin_return_address(0) & EXC_SPSEL;
	struct exception_frame *e;
	if (from_user)
		asm("mrs %0, psp" : "=r" (e));
	else
		e = __builtin_frame_address(0);

#if !defined(CONFIG_MPU)
	dump_exception(from_user, e);
	panic("MemManage");
#else
	if (!from_user) {
		/* kernel faults are always fatal */
		dump_exception(from_user, e);
		panic("MemManage");
	}

	/* try to handle fault */
	const union scb_cfsr cfsr = SCB->CFSR;
	if (cfsr.MMFSR.MMARVALID)
		mpu_fault((void *)SCB->MMFAR, 0);
	else if (cfsr.MMFSR.IACCVIOL)
		mpu_fault((void *)e->ra, 4);
	else if (cfsr.MMFSR.MSTKERR)
		sig_thread(thread_cur(), SIGSEGV); /* stack overflow */
	else
		panic("MemManage");

	/* clear fault */
	write8(&SCB->CFSR.MMFSR, -1);
#endif

	EXCEPTION_EXIT();
}

/*
 * exc_BusFault
 */
void
exc_BusFault(void)
{
	EXCEPTION_ENTRY();

	const bool from_user = (int)__builtin_return_address(0) & EXC_SPSEL;
	struct exception_frame *e;
	if (from_user)
		asm("mrs %0, psp" : "=r" (e));
	else
		e = __builtin_frame_address(0);

	dump_exception(from_user, e);
	if (!from_user)
		panic("BusFault");
	dbg("BusFault");
	sig_thread(thread_cur(), SIGBUS);

	/* clear fault */
	write8(&SCB->CFSR.BFSR, -1);

	EXCEPTION_EXIT();
}

/*
 * exc_UsageFault
 */
__fast_text __attribute__((naked)) void
exc_UsageFault(void)
{
	asm(
		/*
		 * Figure out which stack the exception frame is on:
		 * If we are in thread (user) mode SPSEL is 1 (process stack)
		 * If we are in handler (kernel) mode SPSEL is 0 (main stack)
		 */
		"tst lr,"S(EXC_SPSEL)"\n"
		"ite ne\n"
		"mrsne r0, psp\n"
		"moveq r0, sp\n"		    /* r0 = exception frame */

		/* emulate "mrc 15, 0, rX, cr13, cr0, {3}" */
		"ldr r3, [r0,"S(EFRAME_RA)"]\n"	    /* r3 = address of failed instruction */
		"ldr r1, [r3]\n"		    /* r1 = failed instruction */
		"movw r2, :lower16:0x0f70ee1d\n"
		"movt r2, :upper16:0x0f70ee1d\n"    /* r2 = instruction mask */
		"bics r2, r1\n"			    /* check for mrc get_tls */
		"bne UsageFault\n"
		"ubfx r1, r1, 28, 4\n"		    /* r1 = destination reg */
		"cmp r1, 14\n"
		"bhi UsageFault\n"
		"movw r2, :lower16:active_thread\n"
		"movt r2, :upper16:active_thread\n" /* r2 = &active_thread */
		"ldr r2, [r2]\n"		    /* r2 = active_thread */
		"ldr r2, [r2,"S(THREAD_TLS)"]\n"    /* r2 = tls pointer */

		/*
		 * Skip emulated instruction
		 */
		"add r3, 4\n"
		"str r3, [r0,"S(EFRAME_RA)"]\n"

		/*
		 * Set destination register
		 *
		 * r0 points to exception frame,
		 * r1 is destination register, 0-14,
		 * r2 is value to set
		 */
		"0: tbb [pc, r1]\n"
		".byte (0f-0b-4)/2\n"
		".byte (1f-0b-4)/2\n"
		".byte (2f-0b-4)/2\n"
		".byte (3f-0b-4)/2\n"
		".byte (4f-0b-4)/2\n"
		".byte (5f-0b-4)/2\n"
		".byte (6f-0b-4)/2\n"
		".byte (7f-0b-4)/2\n"
		".byte (8f-0b-4)/2\n"
		".byte (9f-0b-4)/2\n"
		".byte (10f-0b-4)/2\n"
		".byte (11f-0b-4)/2\n"
		".byte (12f-0b-4)/2\n"
		".byte (13f-0b-4)/2\n"
		".byte (14f-0b-4)/2\n"
		".byte 0\n"
		"0: str r2, [r0,"S(EFRAME_R0)"]\n"
		"bx lr\n"
		"1: str r2, [r0,"S(EFRAME_R1)"]\n"
		"bx lr\n"
		"2: str r2, [r0,"S(EFRAME_R2)"]\n"
		"bx lr\n"
		"3: str r2, [r0,"S(EFRAME_R3)"]\n"
		"bx lr\n"
		"4: mov r4, r2\n"
		"bx lr\n"
		"5: mov r5, r2\n"
		"bx lr\n"
		"6: mov r6, r2\n"
		"bx lr\n"
		"7: mov r7, r2\n"
		"bx lr\n"
		"8: mov r8, r2\n"
		"bx lr\n"
		"9: mov r9, r2\n"
		"bx lr\n"
		"10: mov r10, r2\n"
		"bx lr\n"
		"11: mov r11, r2\n"
		"bx lr\n"
		"12: str r2, [r0,"S(EFRAME_R12)"]\n"
		"bx lr\n"
		"13: msr psp, r2\n"
		"bx lr\n"
		"14: str r2, [r0,"S(EFRAME_LR)"]\n"
		"bx lr\n"
	);
}

__attribute__((used)) static void
UsageFault(struct exception_frame *eframe)
{
	EXCEPTION_ENTRY();

	const bool from_user = (int)__builtin_return_address(0) & EXC_SPSEL;
	const union scb_cfsr cfsr = SCB->CFSR;
	const char *what = "Usage Fault\n";
	int sig = SIGILL;

	if (cfsr.UFSR.DIVBYZERO) {
		what = "Divide by Zero\n";
		sig = SIGFPE;
	} if (cfsr.UFSR.UNDEFINSTR)
		what = "Undefined Instruction\n";
	else if (cfsr.UFSR.INVSTATE)
		what = "Invalid State\n";
	else if (cfsr.UFSR.INVPC)
		panic("Invalid PC");		/* Always fatal */
	else if (cfsr.UFSR.UNALIGNED)
		what = "Invalid Unaligned Access\n";
	else if (cfsr.UFSR.NOCP)
		what = "Invalid Coprocessor Access\n";

	/* reset fault register */
	write16(&SCB->CFSR.UFSR, -1);

	dump_exception(from_user, eframe);
	if (!from_user)
		panic(what);
	dbg(what);
	sig_thread(thread_cur(), sig);

	EXCEPTION_EXIT();
}

#if defined(TRACE_SYSCALLS)
void __attribute__((used))
syscall_trace(long r0, long r1, long r2, long r3, long r4, long r5, long r6, long r7)
{
	dbg("SC: tsk %p th %p r0 %08lx r1 %08lx r2 %08lx r3 %08lx r4 %08lx r5 %08lx n %ld\n",
	    task_cur(), thread_cur(), r0, r1, r2, r3, r4, r5, r7);
}
#endif

/*
 * exc_SVCall
 *
 * System call exception
 *
 * Arguments are in r0 -> r5
 * System call number is in r7
 */
__fast_text __attribute__((naked)) void
exc_SVCall(void)
{
	asm(
		/* Preserve non-volatile registers used by the syscall ABI */
		"push {r4, r5, r6, r7}\n"	    /* struct syscall_frame */

		/* Due to late arrival preemption and tail-chaining the values
		   in r0, r1, r2, r3, r12 and lr can be changed between
		   exception stacking and the first instruction of this handler
		   by another higher priority interrupt handler. */
		"mrs ip, psp\n"
		"ldm ip, {r0, r1, r2, r3}\n"

#if defined(TRACE_SYSCALLS)
		"bl syscall_trace\n"
		"mrs ip, psp\n"
		"ldm ip, {r0, r1, r2, r3}\n"
#endif

		/* check if syscall is tabulated */
		"cmp r7, "S(SYSCALL_TABLE_SIZE)"\n"
		"bhi asm_arch_syscall\n"

		/* load handler from table & branch */
		"movw ip, :lower16:syscall_table\n"
		"movt ip, :upper16:syscall_table\n"
		"ldr ip, [ip, r7, lsl 2]\n"	    /* ip = ip[r7 << 2] */
		"cmp ip, 0\n"			    /* check for handler */
		"beq asm_arch_syscall\n"	    /* not supported? */
		"push {r4, r5}\n"		    /* push 5th and 6th args */
		"blx ip\n"			    /* branch to handler */
		"add sp, 8\n"			    /* drop arguments */

	"asm_syscall_retval:\n"
		/* Deliver signals if necessary. */
		"bl sig_deliver\n"

		/* If syscall returned -EPENDSV_RETURN we delivered a signal
		   during return from PendSV. */
		"mov r1, -"S(EPENDSV_RETURN)"\n"
		"cmp r0, r1\n"
		"beq asm_syscall_return\n"

		/* If syscall returned -ERESTARTSYS a previous syscall was
		   interrupted by a signal with SA_RESTART set. This also means
		   that the signal exception frame was popped and psp now
		   points to the frame for the syscall to be restarted. We
		   _NEVER_ return -ERESTARTSYS to userspace as this will trash
		   the exception frame required for syscall restart. We also
		   need to adjust the return address in the exception frame to
		   point to the svc instruction again. */
		"mov r1, -"S(ERESTARTSYS)"\n"
		"cmp r0, r1\n"
		"mrs ip, psp\n"
		"iteee ne\n"			    /* if r0 != -ERESTARTSYS */
		"strne r0, [ip, "S(EFRAME_R0)"]\n"  /* then return r0 */
		"ldreq r0, [ip, "S(EFRAME_RA)"]\n"  /* else load return addr */
		"subeq r0, 2\n"			    /* back up by 2 */
		"streq r0, [ip, "S(EFRAME_RA)"]\n"  /* store return addr */

	"asm_syscall_return:\n"
		/* return to userspace */
		"pop {r4, r5, r6, r7}\n"
		"mov r0, "S(EXC_RETURN_USER)"\n"
		"bx r0\n"

	"asm_arch_syscall:\n"
		"mov r1, r0\n"		/* arch_syscall only takes 1 arg */
		"mov r0, r7\n"
		"bl arch_syscall\n"
		"b asm_syscall_retval\n"

		/* asm_extended_syscall is used for syscalls which
		   1. need to mess with the full register context
		   2. potentially trash the exception frame
		   at entry r0 contains the address of the syscall handler */
	".thumb_func\n"
	".global asm_extended_syscall\n"
	"asm_extended_syscall:\n"
		/* drop r4 and r5 arguments, we need to finish setting up
		   extended_syscall_frame first */
		"add sp, 8\n"

		/* Push the rest of the non-volatile registers. This completes
		 * struct extended_syscall_frame. */
		"push {r8, r9, r10, r11}\n"

		"mov r6, r0\n"			    /* r6 = handler address */

		/* push exception frame */
		"sub sp, "S(EFRAME_SIZE)"\n"
		"mov r0, sp\n"
		"mrs r1, psp\n"
		"mov r2, "S(EFRAME_SIZE)"\n"
		"bl memcpy\n"

		/* setup arguments for syscall handler */
		"push {r4, r5}\n"
		"mrs r0, psp\n"
		"ldm r0, {r0, r1, r2, r3}\n"

		/* branch to syscall handler */
		"blx r6\n"
		"mov r6, r0\n"			    /* r6 = return value */

		/* drop r4 and r5 from stack */
		"add sp, 8\n"

		/* pop exception frame */
		"mrs r0, psp\n"
		"mov r1, sp\n"
		"mov r2, "S(EFRAME_SIZE)"\n"
		"bl memcpy\n"
		"add sp, "S(EFRAME_SIZE)"\n"

		"mov r0, r6\n"			    /* restore return value */

	".thumb_func\n"
	".global asm_extended_syscall_ret\n"
	"asm_extended_syscall_ret:\n"
		"pop {r8, r9, r10, r11}\n"
		"b asm_syscall_retval\n"
	);
}

void
exc_DebugMonitor(void)
{
	panic("DebugMonitor");
}

/*
 * exc_SysTick
 *
 * System timer exception
 */
__fast_text void
exc_SysTick(void)
{
	EXCEPTION_ENTRY();
	timer_tick(1);
	EXCEPTION_EXIT();
}

/*
 * exc_NVIC
 *
 * Nested vectored interrupt controller exception
 */
__fast_text void
exc_NVIC(void)
{
	EXCEPTION_ENTRY();
	int ipsr;
	asm("mrs %0, ipsr" : "=r" (ipsr));
	irq_handler(ipsr - 16);
	EXCEPTION_EXIT();
}

/*
 * exc_Unhandled
 */
void
exc_Unhandled(void)
{
	panic("Unhandled");
}
