#include "locore.h"

#include <arch/mmu.h>
#include <debug.h>

/*
 * handle_interrupt
 */
void
handle_interrupt(uint32_t cause, trap_frame *tf)
{
	const char *s = "Unknown";
	switch (cause) {
	case 0:
		s = "User Software";
		break;
	case 1:
		s = "Supervisor Software";
		break;
	case 3:
		s = "Machine Software";
		break;
	case 4:
		s = "User Timer";
		break;
	case 5:
#ifdef CONFIG_S_MODE
		machine_timer();
		return;
#else
		s = "Supervisor Timer";
		break;
#endif
	case 7:
#ifndef CONFIG_S_MODE
		machine_timer();
		return;
#else
		s = "Machine Timer";
		break;
#endif
	case 8:
		s = "User External";
		break;
	case 9:
#ifdef CONFIG_S_MODE
		machine_irq();
		return;
#else
		s = "Supervisor External";
		break;
#endif
	case 11:
#ifndef CONFIG_S_MODE
		machine_irq();
		return;
#else
		s = "Machine External";
		break;
#endif
	}

	dbg("Interrupt %u (%s) not handled!\n", cause, s);
	panic("Unhandled Interrupt");
}

/*
 * handle_exception
 */
void
handle_exception(uint32_t cause, trap_frame *tf)
{
	const char *s = "Unknown";
	switch (cause) {
	case 0:
		s = "Instruction Address Misaligned";
		break;
	case 1:
#ifdef CONFIG_MPU
		mpu_fault(reinterpret_cast<const void *>(tf->xtval), 4);
		return;
#endif
		s = "Instruction Access Fault";
		break;
	case 2:
		s = "Illegal Instruction";
		break;
	case 3:
		s = "Breakpoint";
		break;
	case 4:
		s = "Load Address Misaligned";
		break;
	case 5:
#ifdef CONFIG_MPU
		mpu_fault(reinterpret_cast<const void *>(tf->xtval), 4);
		return;
#endif
		s = "Load Access Fault";
		break;
	case 6:
		s = "Store/AMO Address Misaligned";
		break;
	case 7:
#ifdef CONFIG_MPU
		mpu_fault(reinterpret_cast<const void *>(tf->xtval), 4);
		return;
#endif
		s = "Store/AMO Access Fault";
		break;
	case 8:
		s = "Environment Call From U-mode";
		break;
	case 9:
		s = "Environment Call From S-mode";
		break;
	case 11:
		s = "Environment Call From M-mode";
		break;
	case 12:
		s = "Instruction Page Fault";
		break;
	case 13:
		s = "Load Page Fault";
		break;
	case 15:
		s = "Store/AMO Page Fault";
		break;
	}

	dbg("Exception %u (%s) not handled!\n", cause, s);
	dbg("ra %08x gp %08x tp %08x sp %08x\n", tf->ra, tf->gp, tf->tp, tf->sp);
	dbg("a0 %08x a1 %08x a2 %08x a3 %08x\n", tf->a[0], tf->a[1], tf->a[2], tf->a[3]);
	dbg("a4 %08x a5 %08x a6 %08x a7 %08x\n", tf->a[4], tf->a[5], tf->a[6], tf->a[7]);
	dbg("s0 %08x s1 %08x s2 %08x s3 %08x\n", tf->s[0], tf->s[1], tf->s[2], tf->s[3]);
	dbg("s4 %08x s5 %08x s6 %08x s7 %08x\n", tf->s[4], tf->s[5], tf->s[6], tf->s[7]);
	dbg("s8 %08x s9 %08x s10 %08x s11 %08x\n", tf->s[8], tf->s[9], tf->s[10], tf->s[11]);
	dbg("t0 %08x t1 %08x t2 %08x t3 %08x\n", tf->t[0], tf->t[1], tf->t[2], tf->t[3]);
	dbg("t4 %08x t5 %08x t6 %08x\n", tf->t[4], tf->t[5], tf->t[6]);
	dbg("xepc %08x xtval %08x xstatus %08x\n", tf->xepc, tf->xtval, tf->xstatus);
	panic("Fatal Exception");
}

/*
 * handle_trap
 */
void
handle_trap(uint32_t cause, trap_frame *tf)
{
	if (cause & 0x80000000)
		handle_interrupt(cause & 0x7fffffff, tf);
	else
		handle_exception(cause, tf);
}
