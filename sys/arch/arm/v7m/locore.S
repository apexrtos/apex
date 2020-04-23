/*
 * locore.S - low level platform support
 */

#include <asm.h>
#include <asm_def.h>
#include <conf/config.h>
#include <cpu.h>
#include <errno.h>
#include <interrupt.h>
#include <syscall_table.h>

// #define TRACE_SYSCALLS

.syntax unified
#if defined(CONFIG_FPU)
.fpu vfp
#endif

/*
 * Interrupt Vector Table
 *
 * Cortex-M3, M4 and M7 require the vector table to be aligned to the next
 * power of two greater than the offset of the highest vector in use.
 */
#if CONFIG_IRQS <= 16
#define ALIGN 7
#elif CONFIG_IRQS <= 48
#define ALIGN 8
#elif CONFIG_IRQS <= 112
#define ALIGN 9
#elif CONFIG_IRQS <= 240
#define ALIGN 10
#elif CONFIG_IRQS <= 496
#define ALIGN 11
#else
#error ARMv7-M only supports up to 496 external interrupts
#endif
.section .vectors, "ax"
.align ALIGN
.global vectors
vectors:
	.word v7m_entry			/* SP_main */
	.word v7m_entry			/* Reset */
	.word v7m_entry			/* NMI */
	.word v7m_entry			/* HardFault */
	.word v7m_entry			/* MemManage */
	.word v7m_entry			/* BusFault */
	.word v7m_emulate		/* UsageFault */
	.word v7m_entry			/* 0x1c Reserved */
	.word v7m_entry			/* 0x20 Reserved */
	.word v7m_entry			/* 0x24 Reserved */
	.word v7m_entry			/* 0x28 Reserved */
	.word exc_SVCall		/* SVCall */
	.word v7m_entry			/* DebugMonitor */
	.word v7m_entry			/* 0x34 Reserved */
	.word exc_PendSV		/* PendSV */
	.word exc_SysTick		/* SysTick */
	.rept CONFIG_IRQS
	.word exc_NVIC
	.endr

/*
 * Begin text
 */
.text

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
.thumb_func
.global _start
_start:
	/* set r5 to system control space address */
	mov r5, A_SCS

	/* set vector table address */
	ldr r4, =vectors
	str r4, [r5, A_VTOR-A_SCS]

	/* enable UsageFault, BusFault and MemManage exceptions */
	ldr r4, [r5, A_SHCSR-A_SCS]
	orr r4, (SHCSR_USGFAULTNA|SHCSR_BUSFAULTENA|SHCSR_MEMFAULTENA)
	str r4, [r5, A_SHCSR-A_SCS]

	/* trap divide by zero, 8-byte stack alignment on exception */
	ldr r4, [r5, A_CCR-A_SCS]
	orr r4, CCR_DIV_0_TRP|CCR_STKALIGN
	str r4, [r5, A_CCR-A_SCS]

	/* set SVC and PendSV priority */
	mov r4, (IPL_SVCALL << 24)
	str r4, [r5, A_SHPR2-A_SCS]	/* SVC */
	mov r4, (IPL_PENDSV << 16)
	str r4, [r5, A_SHPR3-A_SCS]	/* PendSV */

	/* use process stack in thread mode */
	mov r4, CONTROL_SPSEL
	msr control, r4

#ifdef CONFIG_FPU
	/* enable access to FPU */
	mov r4, 0xf00000                /* CP10, CP11 full access */
	str r4, [r5, A_CPACR-A_SCS]
#endif

	/* setup stack pointers */
	ldr sp, =__stack_top
	ldr r4, =__irqstack_top
	msr msp, r4
	mov lr, 0xffffffff

	/* wait for writes to system control space to complete */
	dsb

	/* kernel_main(archive_addr, archive_size, machdep0, machdep1) */
	b kernel_main
.ltorg

/*
 * Begin fast_text
 */
.section .fast_text, "ax"

/*
 * v7m_entry
 *
 * Entry point for most exceptions. Get exception frame and dispatch to next
 * level handler.
 */
.thumb_func
.global v7m_entry
v7m_entry:
	tst lr, EXC_SPSEL
	mrsne r0, psp			/* r0 = exception frame */
	movne r1, 0			/* r1 = 0 (thread mode) */
	moveq r0, sp			/* r0 = exception frame */
	moveq r1, 1			/* r1 = 1 (handler mode) */

	/* dispatch exception */
	mrs r2, ipsr			/* r2 = exception number */
0:	tbb [pc, r2]			/* switch (r2) */
	.byte (0f-0b-4)/2
	.byte (1f-0b-4)/2
	.byte (2f-0b-4)/2
	.byte (3f-0b-4)/2
	.byte (4f-0b-4)/2
	.byte (5f-0b-4)/2
	.byte (6f-0b-4)/2
	.byte (7f-0b-4)/2
	.byte (8f-0b-4)/2
	.byte (9f-0b-4)/2
	.byte (10f-0b-4)/2
	.byte (11f-0b-4)/2
	.byte (12f-0b-4)/2
	.byte (13f-0b-4)/2
	.byte (14f-0b-4)/2
	.byte (15f-0b-4)/2
0:					/* SP_main */
1:					/* Reset */
2:					/* NMI */
3:	b exc_Unhandled			/* HardFault */
4:	b exc_MemManage			/* MemManage */
5:	b exc_BusFault			/* BusFault */
6:	b exc_UsageFault		/* UsageFault */
7:					/* Reserved */
8:					/* Reserved */
9:					/* Reserved */
10:					/* Reserved */
11:					/* SVCall */
12:					/* DebugMonitor */
13:					/* Reserved */
14:					/* PendSV */
15:	b exc_Unhandled			/* SysTick */

/*
 * exc_SVCall - system call exception
 *
 * Arguments are passed in r0->r6, system call number is in r7.
 */
.thumb_func
exc_SVCall:
	/* Due to late arrival preemption and tail chaining the values
	   in r0, r1, r2, r3, r12 and lr can be changed between exception
	   stacking and the first instruction of this handler by another
	   higher priority handler. */

	/* syscall_ret uses svc from privileged thread for userspace return */
	mrs r3, control			/* r3 = control */
	tst r3, CONTROL_NPRIV		/* if (!(r3 & CONTROL_NPRIV)) */
	beq syscall_ret_handler		/*	goto syscall_ret_handler */

#if defined(TRACE_SYSCALLS)
	push {r3, lr}
	mrs r0, psp
	ldm r0, {r0-r3}
	push {r4, r5, r6, r7}
	bl syscall_trace
	add sp, 16
	pop {r3, lr}
#endif

	/* switch to kernel stack */
	movw r0, :lower16:active_thread
	movt r0, :upper16:active_thread	/* r0 = &active_thread */
	ldr r0, [r0]			/* r0 = active_thread */
	mrs r2, psp			/* r2 = psp (user stack pointer) */
	str r2, [r0, THREAD_CTX_USTACK]	/* active_thread->ctx.ustack = r2 */
	ldr ip, [r0, THREAD_CTX_ESTACK]	/* ip = active_thread->ctx.estack */
	stmdb ip!, {r3-r11, lr}		/* push nvregs */
	stmdb ip!, {r4, r5, r6, r7}	/* push syscall_args */

	/* load syscall handler address into r6 */
	cmp r7, SYSCALL_TABLE_SIZE	/* if (r7 >= SYSCALL_TABLE_SIZE) */
	bhi .Larch_syscall		/*	goto .Larch_syscall */
	movw r0, :lower16:syscall_table
	movt r0, :upper16:syscall_table
	ldr r6, [r0, r7, lsl 2]		/* r6 = r0[r7 << 2] */
	cmp r6, 0			/* if (r6 == NULL) */
	beq .Larch_syscall		/*	goto .Larch_syscall */

.Lenter_kernel:
	bic r3, CONTROL_NPRIV
	msr control, r3			/* set kernel thread mode privilege */

	/* push exception_frame_basic for entry to kernel syscall handler */
	ldm r2, {r0-r3}			/* r0-r3 = syscall args */
	movw r5, :lower16:syscall_ret	/* r12 = random value */
	movt r5, :upper16:syscall_ret   /* r5(lr) = syscall_ret */
	mov r7,	EPSR_T			/* r6(ra) = syscall handler */
	stmdb ip!, {r0-r7}		/* r7(xpsr) */
	msr psp, ip			/* set stack pointer */
	mov r0, EXC_RETURN_THREAD_PROCESS_BASIC
	bx r0				/* return to thread mode */

.Larch_syscall:
	/* index outside table bounds or null handler */
	movw r6, :lower16:arch_syscall
	movt r6, :upper16:arch_syscall
	b .Lenter_kernel

/*
 * syscall_ret - return from system call to userspace
 *
 * r0 = syscall return value
 */
.thumb_func
.global syscall_ret
syscall_ret:
	mov r4, r0			/* preserve r0 (syscall return value) */
#if defined(CONFIG_FPU)
	mov r0, CONTROL_SPSEL		/* clear FPCA in case syscall used */
	msr control, r0			/* floating point instructions */
#endif
	svc 0				/* switch to handler mode */

/*
 * syscall_ret_handler - return from system call to userspace (handler mode)
 *
 * r4 = syscall return value
 */
syscall_ret_handler:
	mov r0, r4			/* restore r0 */
	movw ip, :lower16:active_thread
	movt ip, :upper16:active_thread	/* ip = &active_thread */
	ldr r4, [ip]			/* r4 = active_thread */
	/* fall through to return_to_user */

/*
 * return_to_user - return to userspace after context switch or syscall
 *
 * r0 = syscall return value
 * r4 = active_thread
 */
return_to_user:
	bl sig_deliver			/* deliver unblocked pending signals */
#if defined(CONFIG_MPU)
	mov r5, r0			/* preserve return value */
	bl mpu_user_thread_switch	/* switch mpu context */
	mov r0, r5
#endif
	ldr r2, [r4, THREAD_CTX_USTACK]	/* r2 = active_thread->ctx.ustack */

	/* There are three cases where r0 == -EINTERRUPT_RETURN here:
	    1. returing to userspace from interrupt via PendSV
	    2. returning from asynchronous signal via sigreturn
	    3. thread is terminating
	   In all cases there is no value to return to userspace. */
	mov r1, -EINTERRUPT_RETURN
	cmp r0, r1
	beq .Lout

	/* If sigreturn returned -ERESTARTSYS a syscall was interrupted by a
	   signal with SA_RESTART set. Adjust return address to restart. */
	mov r1, -ERESTARTSYS
	cmp r0, r1			/* if (r0 != -ERESTARTSYS) */
	strne r0, [r2, EFRAME_R0]	/*	    then return r0 */
	ldreq r1, [r2, EFRAME_RA]	/* else load return addr */
	subeq r1, 2			/*	    back up by 2 */
	streq r1, [r2, EFRAME_RA]	/*	    store return addr */

.Lout:
	ldr r5, [r4, THREAD_CTX_ESTACK]	/* r5 = active_thread->ctx.estack */
	ldmdb r5, {r3-r11, lr}		/* pop nvregs */
	msr control, r3			/* restore control register */
	msr psp, r2			/* switch back to user stack */
#if defined(TRACE_SYSCALLS)
	mov r1, r7
	push {r0, lr}
	bl syscall_trace_return
	pop {r0, lr}
#endif
	bx lr				/* return to thread mode */

/*
 * exc_PendSV - context switch exception
 */
.thumb_func
exc_PendSV:
	movw ip, :lower16:active_thread
	movt ip, :upper16:active_thread	/* ip = &active_thread */
	ldr ip, [ip]			/* ip = active_thread */

	/* user threads switch to kstack */
	mrs r0, psp			/* r0 = psp (kernel or user sp) */
	mrs r3, control
	tst r3, CONTROL_NPRIV		/* if (!(r3 & CONTROL_NPRIV)) */
	beq .Lon_kstack			/*	goto .Lon_kstack */
	str r0, [ip, THREAD_CTX_USTACK]	/* active_thread->ctx.ustack = r0 */
	ldr r0, [ip, THREAD_CTX_ESTACK]	/* r0 = active_thread->ctx.estack */

.Lon_kstack:
	stmdb r0!, {r3-r11, lr}		/* push nvregs */
#if defined(CONFIG_FPU)
	vstmdb r0!, {s16-s31}		/* push fpu_nvregs */
#endif
	str r0, [ip, THREAD_CTX_KSTACK]	/* active_thread->ctx.kstack = r0 */

	cpsid i
	bl sch_switch			/* switch threads */
	cpsie i

	movw ip, :lower16:active_thread
	movt ip, :upper16:active_thread	/* ip = &active_thread */
	ldr r4, [ip]			/* r4 = active_thread */
	ldr r1, [r4, THREAD_CTX_KSTACK]	/* r1 = active_thread->ctx.kstack */
#if defined(CONFIG_FPU)
	vldmia r1!, {s16-s31}		/* pop fpu_nvregs */
#endif
	ldr r0, [r1]			/* r0 = saved control register */
	tst r0, CONTROL_NPRIV		/* if (r0 & CONTROL_NPRIV) */
	movne r0, -EINTERRUPT_RETURN	/*	r0 = -EINTERRUPT_RETURN */
	bne return_to_user		/*	goto return_to_user */

	/* return to kernel thread */
	ldmia r1!, {r3-r11, lr}		/* load nvregs */
	msr control, r3			/* restore control register */
	msr psp, r1			/* set stack pointer */
	bx lr				/* return to thread mode */
