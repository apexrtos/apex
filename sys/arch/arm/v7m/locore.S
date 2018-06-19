/*
 * locore.S - low level platform support
 */

#include <asm.h>
#include <conf/config.h>
#include <cpu.h>
#include <interrupt.h>

.syntax unified

/*
 * Interrupt Vector Table
 * Alignment is guaranteed by link script
 */
.text
.global vectors
vectors:
	.word 0				/* SP_main */
	.word 0				/* Reset */
	.word exc_NMI
	.word exc_HardFault
	.word exc_MemManage
	.word exc_BusFault
	.word exc_UsageFault
	.word 0				/* 0x1c Reserved */
	.word 0				/* 0x20 Reserved */
	.word 0				/* 0x24 Reserved */
	.word 0				/* 0x28 Reserved */
	.word exc_SVCall
	.word exc_DebugMonitor
	.word 0				/* 0x34 Reserved */
	.word exc_PendSV
	.word exc_SysTick
	.rept CONFIG_IRQS
	.word exc_NVIC
	.endr

/*
 * _start
 *
 * Kernel entry point
 *
 * Assumes the following state:
 *   CPU is in Thumb, Thread, Privileged mode
 *   Interrupts are disabled and all interrupt sources are disabled
 *   The stack pointer is somewhere in BOOTSTACK
 *   Clocks and caches are apropriately configured
 *   r0 contains the address of bootinfo
 */
.text
.thumb_func
.global _start
_start:
	/* setup a temporary vector table for the transition to handler mode
	   using the top of bootstack */
	ldr r1, =CONFIG_BOOTSTACK_BASE_PHYS
	adr r2, 1f + 1		/* address of SVC handler */
	str r2, [r1, 11 * 4]	/* store to temporary vector table */
	dsb			/* make sure vector is stored */

	/* set r2 to system control space address */
	mov r2, A_SCS

	/* set vector table address to temporary table */
	str r1, [r2, A_VTOR-A_SCS]

	/* execute SVC */
	cpsie i
	svc 0

	/* set real vector table address */
1:	cpsid	i
	adr r1, vectors
	str r1, [r2, A_VTOR-A_SCS]

	/* set thread mode to unprivileged */
	mov r1, CONTROL_NPRIV
	msr control, r1
	isb

	/* clear user mode stack as we will become a kernel thread */
	mov r1, 0
	msr psp, r1

	/* enable UsageFault, BusFault and MemManage exceptions */
	ldr r1, [r2, A_SHCSR-A_SCS]
	orr r1, (SHCSR_USGFAULTNA|SHCSR_BUSFAULTENA|SHCSR_MEMFAULTENA)
	str r1, [r2, A_SHCSR-A_SCS]

	/* set SVC and PendSV priority */
	mov r5, (IPL_SVCALL << 24)
	str r5, [r2, A_SHPR2-A_SCS]	/* SVC to 255 */
	mov r5, (IPL_PENDSV << 16)
	str r5, [r2, A_SHPR3-A_SCS]	/* PendSV to 255 */

	/* enable access to FPU */
#ifdef CONFIG_FPU
	#error Enable CP10 and CP11 access in CPACR
#endif

	/* copy bootinfo to final location */
	bl copy_bootinfo

	/* reset stack pointer */
	ldr sp, =(CONFIG_BOOTSTACK_BASE_PHYS + CONFIG_BOOTSTACK_SIZE)
	mov lr, 0xffffffff

	/* jump to kernel */
	b kernel_main
.ltorg
