/*
 * locore.S - low level platform support
 */

#include <asm_def.h>
#include <conf/config.h>
#include <errno.h>
#include <syscall_table.h>

/*
 * _start
 *
 * Kernel entry point
 *
 * Assumes the following state:
 *   If CONFIG_S_MODE CPU is in S mode, otherwise CPU is in M mode
 *   Interrupts are disabled
 *   Clocks and caches are appropriately configured
 *   r0 = archive_addr
 *   r1 = archive_size
 *   r2 = machdep0
 *   r3 = machdep1
 */
.text
.global _start
_start:
	/* set global pointer */
.option push
.option norelax
	la gp, __global_pointer
.option pop

	/* set stack pointer */
	la sp, __stack_top

	/* setup interrupts */
#ifdef CONFIG_S_MODE
	#error TODO: implement S mode
#else
	la t0, trap_entry
	csrw mtvec, t0
	li t0, MIE_MEIE | MIE_MTIE
	csrw mie, t0
#endif

	/* kernel_main(archive_addr, archive_size, machdep0, machdep1) */
	tail kernel_main

/*
 * trap_entry - entry point for all traps
 */
.section .fast_text, "ax"
.global trap_entry
.align 2				/* must be 4-byte aligned */
trap_entry:
#ifdef CONFIG_S_MODE
	csrrw tp, sscratch, tp		/* swap tp and sscratch */
#else
	csrrw tp, mscratch, tp		/* swap tp and mscratch */
#endif
	beqz tp, .Ltrap_from_kernel	/* scratch == 0 if trap from kernel */

	/* from userspace */
	sw sp, THREAD_CTX_XSP(tp)	/* save current stack pointer */
	lw sp, THREAD_CTX_KSTACK(tp)	/* switch to kstack if from user mode */
	j .Lsave_trap_frame

	/* from kernel */
.Ltrap_from_kernel:
	csrr tp, mscratch		/* restore tp */
	sw sp, THREAD_CTX_XSP(tp)	/* save current stack pointer */

.Lsave_trap_frame:
	addi sp, sp, -TRAP_FRAME_SIZE
	sw ra, TRAP_FRAME_RA(sp)
	sw gp, TRAP_FRAME_GP(sp)
	sw a0, TRAP_FRAME_A0(sp)
	sw a1, TRAP_FRAME_A1(sp)
	sw a2, TRAP_FRAME_A2(sp)
	sw a3, TRAP_FRAME_A3(sp)
	sw a4, TRAP_FRAME_A4(sp)
	sw a5, TRAP_FRAME_A5(sp)
	sw a6, TRAP_FRAME_A6(sp)
	sw a7, TRAP_FRAME_A7(sp)
	sw s0, TRAP_FRAME_S0(sp)
	sw s1, TRAP_FRAME_S1(sp)
	sw s2, TRAP_FRAME_S2(sp)
	sw s3, TRAP_FRAME_S3(sp)
	sw s4, TRAP_FRAME_S4(sp)
	sw s5, TRAP_FRAME_S5(sp)
	sw s6, TRAP_FRAME_S6(sp)
	sw s7, TRAP_FRAME_S7(sp)
	sw s8, TRAP_FRAME_S8(sp)
	sw s9, TRAP_FRAME_S9(sp)
	sw s10, TRAP_FRAME_S10(sp)
	sw s11, TRAP_FRAME_S11(sp)
	sw t0, TRAP_FRAME_T0(sp)
	sw t1, TRAP_FRAME_T1(sp)
	sw t2, TRAP_FRAME_T2(sp)
	sw t3, TRAP_FRAME_T3(sp)
	sw t4, TRAP_FRAME_T4(sp)
	sw t5, TRAP_FRAME_T5(sp)
	sw t6, TRAP_FRAME_T6(sp)
#ifdef CONFIG_S_MODE
	csrr s0, sepc
	csrr t1, stval
	csrr t2, sstatus
	csrrw t3, sscratch, zero	/* get original tp, zero scratch */
	csrr t6, scause
#else
	csrr s0, mepc
	csrr t1, mtval
	csrr t2, mstatus
	csrrw t3, mscratch, zero	/* get original tp, zero scratch */
	csrr t6, mcause
#endif
	sw s0, TRAP_FRAME_xEPC(sp)
	sw t1, TRAP_FRAME_xTVAL(sp)
	sw t2, TRAP_FRAME_xSTATUS(sp)
	sw t3, TRAP_FRAME_TP(sp)
	lw t0, THREAD_CTX_XSP(tp)
	sw t0, TRAP_FRAME_SP(sp)	    /* save original stack pointer */

	/* set global pointer for kernel */
.option push
.option norelax
	la gp, __global_pointer
.option pop

	/* set thread pointer for kernel */
	/* TODO: use this to optimise sch_active() calls? */
	la tp, active_thread
	lw tp, 0(tp)

	/* handle syscalls */
	li t0, 8
	beq t6, t0, .Lhandle_syscall

	/* increment irq nesting counter */
	lw t0, THREAD_CTX_IRQ_NESTING(tp)
	addi t0, t0, 1
	sw t0, THREAD_CTX_IRQ_NESTING(tp)

	/* handle all other traps (interrupts & exceptions) */
	mv a0, t6
	mv a1, sp
	la ra, trap_return
	tail handle_trap

.Lhandle_syscall:
	/* enable interrupts before calling handler */
#ifdef CONFIG_S_MODE
	csrs sstatus, SSTATUS_SIE
#else
	csrs mstatus, MSTATUS_MIE
#endif
	/* advance pc so we don't return to and repeat scall instruction */
	addi s0, s0, 4
	sw s0, TRAP_FRAME_xEPC(sp)
	/* syscall number is in a7 */
	li t0, SYSCALL_TABLE_SIZE
	bgeu a7, t0, .Larch_syscall
	la t0, syscall_table
	slli t1, a7, 2			    /* t1 = syscall_nr * 4 */
	add t0, t0, t1			    /* t0 = syscall_table + t1 */
	lw t0, 0(t0)			    /* t0 = handler address */
	beqz t0, .Larch_syscall		    /* null handler? */

.Lrun_syscall_handler:
	/* handler address is in t0 */
	la ra, return_to_user
	jr t0

.Larch_syscall:
	/* index outside table bounds or null handler */
	la t0, arch_syscall
	j .Lrun_syscall_handler

/*
 * return_to_user
 *
 * a0 = syscall return value
 */
.section .fast_text, "ax"
.global return_to_user
return_to_user:
	/* deliver signals */
	call sig_deliver
	mv s0, a0
#ifdef CONFIG_MPU
	call mpu_user_thread_switch		    /* switch mpu context */
#endif

	/* There are two cases where s0 == -EINTERRUPT_RETURN here:
	    1. returing to userspace from trap
	    2. returning from asynchronous signal via sigreturn
	   In both cases there is no value to return to userspace. */
	li t0, -EINTERRUPT_RETURN
	beq s0, t0, .Lreturn_to_user_final

	/* If sigreturn returned -ERESTARTSYS a syscall was interrupted by a
	   signal with SA_RESTART set. Adjust return address to restart. */
	li t0, -ERESTARTSYS
	beq s0, t0, .Lrestart_syscall

	/* Otherwise, set return value. */
	sw s0, TRAP_FRAME_A0(sp)
	j .Lreturn_to_user_final

.Lrestart_syscall:
	lw t0, TRAP_FRAME_xEPC(sp)
	addi t0, t0, -4
	sw t0, TRAP_FRAME_xEPC(sp)

.Lreturn_to_user_final:
#ifdef CONFIG_S_MODE
	csrci sstatus, SSTATUS_SIE	    /* disable interrupts */
	csrw sscratch, tp		    /* scrach is tp for user threads */
#else
	csrci mstatus, MSTATUS_MIE	    /* disable interrupts */
	csrw mscratch, tp		    /* scrach is tp for user threads */
#endif
	tail restore_trap_frame

/*
 * trap_return - return from trap
 */
.section .fast_text, "ax"
.global trap_return
trap_return:
	lw t0, TRAP_FRAME_xSTATUS(sp)
#ifdef CONFIG_S_MODE
	andi s0, t0, SSTATUS_SPP	    /* SPP == 0 on return to userspace */
	csrci sstatus, SSTATUS_SIE	    /* disable interrupts */
#else
	li t1, MSTATUS_MPP
	and s0, t0, t1			    /* MPP == 0 on return to userspace */
	csrci mstatus, MSTATUS_MIE	    /* disable interrupts */
#endif

	/* decrement irq nesting and check for nested irq */
	lw t0, THREAD_CTX_IRQ_NESTING(tp)
	addi t0, t0, -1
	sw t0, THREAD_CTX_IRQ_NESTING(tp)
	bnez t0, restore_trap_frame	    /* irq_nesting != 0: nested irq */

	/* check if we're returning to userspace */
	li a0, -EINTERRUPT_RETURN
	beqz s0, return_to_user

	/* otherwise we're returning to kernel */
	call sch_switch			    /* reschedule if necessary */
	tail restore_trap_frame

/*
 * restore_trap_frame
 */
.section .fast_text, "ax"
.global restore_trap_frame
restore_trap_frame:
	lw t0, TRAP_FRAME_xSTATUS(sp)
	lw t1, TRAP_FRAME_xEPC(sp)
#ifdef CONFIG_S_MODE
	csrw sstatus, t0	    /* interrupts disabled in trap frame */
	csrw sepc, t1
#else
	csrw mstatus, t0	    /* interrupts disabled in trap frame */
	csrw mepc, t1
#endif
	lw ra, TRAP_FRAME_RA(sp)
	lw gp, TRAP_FRAME_GP(sp)
	lw a0, TRAP_FRAME_A0(sp)
	lw a1, TRAP_FRAME_A1(sp)
	lw a2, TRAP_FRAME_A2(sp)
	lw a3, TRAP_FRAME_A3(sp)
	lw a4, TRAP_FRAME_A4(sp)
	lw a5, TRAP_FRAME_A5(sp)
	lw a6, TRAP_FRAME_A6(sp)
	lw a7, TRAP_FRAME_A7(sp)
	lw s0, TRAP_FRAME_S0(sp)
	lw s1, TRAP_FRAME_S1(sp)
	lw s2, TRAP_FRAME_S2(sp)
	lw s3, TRAP_FRAME_S3(sp)
	lw s4, TRAP_FRAME_S4(sp)
	lw s5, TRAP_FRAME_S5(sp)
	lw s6, TRAP_FRAME_S6(sp)
	lw s7, TRAP_FRAME_S7(sp)
	lw s8, TRAP_FRAME_S8(sp)
	lw s9, TRAP_FRAME_S9(sp)
	lw s10, TRAP_FRAME_S10(sp)
	lw s11, TRAP_FRAME_S11(sp)
	lw t0, TRAP_FRAME_T0(sp)
	lw t1, TRAP_FRAME_T1(sp)
	lw t2, TRAP_FRAME_T2(sp)
	lw t3, TRAP_FRAME_T3(sp)
	lw t4, TRAP_FRAME_T4(sp)
	lw t5, TRAP_FRAME_T5(sp)
	lw t6, TRAP_FRAME_T6(sp)
	lw tp, TRAP_FRAME_TP(sp)
	lw sp, TRAP_FRAME_SP(sp)
#ifdef CONFIG_S_MODE
	sret
#else
	mret
#endif

/*
 * context_switch(thread *prev, thread *next)
 *
 * Interrupts are disabled.
 */
.section .fast_text, "ax"
.global _Z14context_switchP6threadS0_
_Z14context_switchP6threadS0_:
	/* save context */
	addi sp, sp, -CONTEXT_FRAME_SIZE	/* push context_frame */
	sw sp, THREAD_CTX_KSP(a0)
	sw ra, CONTEXT_FRAME_RA(sp)
	sw s0, CONTEXT_FRAME_S0(sp)
	sw s1, CONTEXT_FRAME_S1(sp)
	sw s2, CONTEXT_FRAME_S2(sp)
	sw s3, CONTEXT_FRAME_S3(sp)
	sw s4, CONTEXT_FRAME_S4(sp)
	sw s5, CONTEXT_FRAME_S5(sp)
	sw s6, CONTEXT_FRAME_S6(sp)
	sw s7, CONTEXT_FRAME_S7(sp)
	sw s8, CONTEXT_FRAME_S8(sp)
	sw s9, CONTEXT_FRAME_S9(sp)
	sw s10, CONTEXT_FRAME_S10(sp)
	sw s11, CONTEXT_FRAME_S11(sp)

	/* load context */
	lw sp, THREAD_CTX_KSP(a1)
	lw ra, CONTEXT_FRAME_RA(sp)
	lw s0, CONTEXT_FRAME_S0(sp)
	lw s1, CONTEXT_FRAME_S1(sp)
	lw s2, CONTEXT_FRAME_S2(sp)
	lw s3, CONTEXT_FRAME_S3(sp)
	lw s4, CONTEXT_FRAME_S4(sp)
	lw s5, CONTEXT_FRAME_S5(sp)
	lw s6, CONTEXT_FRAME_S6(sp)
	lw s7, CONTEXT_FRAME_S7(sp)
	lw s8, CONTEXT_FRAME_S8(sp)
	lw s9, CONTEXT_FRAME_S9(sp)
	lw s10, CONTEXT_FRAME_S10(sp)
	lw s11, CONTEXT_FRAME_S11(sp)
	addi sp, sp, CONTEXT_FRAME_SIZE		/* pop context_frame */

	/* set thread pointer */
	mv tp, a1

	ret

/*
 * thread_entry - entry point for new threads
 *
 * s0 = function to branch to
 * s1 = argument to function
 * s2 = initial xstatus value
 */
.text
.global thread_entry
thread_entry:
#ifdef CONFIG_S_MODE
	csrw sstatus, s2
#else
	csrw mstatus, s2
#endif
	mv a0, s1
	jalr s0
