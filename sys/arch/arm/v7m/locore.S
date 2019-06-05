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
 */
.section .vectors
.align 7				/* 128-byte alignment */
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
 *   The stack pointer is unknown
 *   Clocks and caches are appropriately configured
 *   r0 = archive_addr
 *   r1 = archive_size
 *   r2 = machdep0
 *   r3 = machdep1
 */
.text
.thumb_func
.global _start
_start:
	/* set stack pointer */
	ldr sp, =__stack_top

	/* setup a temporary vector table for the transition to handler mode */
	ldr r4, =__stack_start	/* use stack for temporary table */
	add r4, r4, 127		/* align to 128-byte boundary */
	bic r4, r4, 127
	adr r5, 1f + 1		/* address of SVC handler */
	str r5, [r4, 11 * 4]	/* store to temporary vector table */
	dsb			/* make sure vector is stored */

	/* set r5 to system control space address */
	mov r5, A_SCS

	/* set vector table address to temporary table */
	str r4, [r5, A_VTOR-A_SCS]

	/* execute SVC */
	cpsie i
	svc 0

	/* set real vector table address */
1:	cpsid	i
	ldr r4, =vectors
	str r4, [r5, A_VTOR-A_SCS]

	/* set thread mode to unprivileged */
	mov r4, CONTROL_NPRIV
	msr control, r4
	isb

	/* clear user mode stack as we will become a kernel thread */
	mov r4, 0
	msr psp, r4

	/* enable UsageFault, BusFault and MemManage exceptions */
	ldr r4, [r5, A_SHCSR-A_SCS]
	orr r4, (SHCSR_USGFAULTNA|SHCSR_BUSFAULTENA|SHCSR_MEMFAULTENA)
	str r4, [r5, A_SHCSR-A_SCS]

	/* trab divide by zero, 8-byte stack alignment on exception */
	ldr r4, [r5, A_CCR-A_SCS]
	orr r4, CCR_DIV_0_TRP|CCR_STKALIGN
	str r4, [r5, A_CCR-A_SCS]

	/* set SVC and PendSV priority */
	mov r4, (IPL_SVCALL << 24)
	str r4, [r5, A_SHPR2-A_SCS]	/* SVC to 255 */
	mov r4, (IPL_PENDSV << 16)
	str r4, [r5, A_SHPR3-A_SCS]	/* PendSV to 255 */

	/* enable access to FPU */
#ifdef CONFIG_FPU
	mov r4, 0xf00000		/* CP10, CP11 full access */
	str r4, [r5, A_CPACR-A_SCS]
#endif

	/* reset stack pointer to drop exception frame from svc */
	ldr sp, =__stack_top
	mov lr, 0xffffffff

	/* wait for writes to system control space to complete */
	dsb

	/* kernel_main(archive_addr, archive_size, machdep0, machdep1) */
	b kernel_main
.ltorg
