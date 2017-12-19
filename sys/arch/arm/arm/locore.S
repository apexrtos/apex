/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * locore.S - low level platform support
 */

#include <conf/config.h>
#include <machine/asm.h>
#include <syspage.h>
#include <platform.h>
#include <mmu.h>	/* for UMEM_MAX */
#include <cpu.h>

	.section ".text","ax"
	.code 32

/*
 * Kernel start point
 */
ENTRY(kernel_start)
#ifdef CONFIG_MMU
	b	reset_entry		/* Relative jump */
#endif
vector_start:
	/*
	 * Exception vector
	 *
	 * This table will be copied to an appropriate location.
	 * (the location is platform specific.)
	 */
	ldr	pc, reset_target	/* 0x00 mode: svc */
	ldr	pc, undefined_target	/* 0x04 mode: ? */
	ldr	pc, swi_target		/* 0x08 mode: svc */
	ldr	pc, prefetch_target	/* 0x0c mode: abort */
	ldr	pc, abort_target	/* 0x10 mode: abort */
	nop				/* 0x14 reserved */
	ldr	pc, irq_target		/* 0x18 mode: irq */
	ldr	pc, fiq_target		/* 0x1c mode: fiq */

reset_target:		.word	reset_entry
undefined_target:	.word	undefined_entry
swi_target:		.word	syscall_entry
prefetch_target:	.word	prefetch_entry
abort_target:		.word	abort_entry
irq_target:		.word	interrupt_entry
fiq_target:		.word	fiq_entry

vector_end:

	.global bootinfo
bootinfo:		.word	BOOTINFO_BASE
boot_stack:		.word	BOOTSTACK_TOP
int_stack:		.word	INTSTACK_TOP - 0x100
irq_mode_stack:		.word	INTSTACK_TOP
sys_mode_stack:		.word	SYSSTACK_TOP
abort_mode_stack:	.word	ABTSTACK_TOP
irq_nest_count:		.word	irq_nesting
#ifdef CONFIG_MMU
reload_pc_target:	.word	reload_pc
#endif
reset_entry:
	/*
	 * Setup stack pointer for each processor mode
	 */
	mov	r0, #(PSR_IRQ_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr_c, r0
	ldr	sp, irq_mode_stack	/* Set IRQ mode stack */

	mov	r0, #(PSR_UND_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, abort_mode_stack	/* Set Undefined mode stack */

	mov	r0, #(PSR_ABT_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, abort_mode_stack	/* Set Abort mode stack */

	mov	r0, #(PSR_SYS_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, sys_mode_stack	/* Set SYS mode stack */

	mov	r0, #(PSR_SVC_MODE|PSR_FIQ_DIS|PSR_IRQ_DIS)
	msr	cpsr, r0
	ldr	sp, boot_stack		/* Set SVC mode stack */

	/* It's svc mode now. */

#ifdef CONFIG_MMU
	/*
	 * Setup control register
	 */
	mov	r0, #CTL_DEFAULT
	mcr	p15, 0, r0, c1, c0, 0

	/*
	 * Initialize page table
	 * The physical address 0-4M is mapped on virtual address 2G.
	 */
	mov	r1, #BOOT_PGD_PHYS		/* Clear page directory */
	mov	r2, #(BOOT_PGD_PHYS + 0x4000)	/* +16k */
	mov	r0, #0
1:	str	r0, [r1], #4
	teq	r1, r2
	bne	1b

	mov	r1, #(BOOT_PGD_PHYS + 0x2000) /* Set PTE0 address in pgd */
	mov	r0, #BOOT_PTE0_PHYS	/* WBUF/CACHE/SYSTEM attribute */
	orr	r0, #0x03
	str	r0, [r1]

	mov	r1, #BOOT_PTE0_PHYS	/* Fill boot page table entry */
	add	r2, r1, #0x1000
	mov	r0, #0x1e
1:	str	r0, [r1], #4
	add	r0, #0x1000
	teq	r1, r2
	bne	1b

	/*
	 * Enable paging
	 * The physical address 0-4M is temporarily mapped to virtial
	 * address 0-4M. This is needed to enable paging.
	 */
	mov	r1, #BOOT_PGD_PHYS	/* Set PTE0 address in pgd */
	mov	r0, #BOOT_PTE0_PHYS	/* WBUF/CACHE/SYSTEM attribute */
	orr	r0, #0x03
	str	r0, [r1]

	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mcr	p15, 0, r0, c8, c7, 0	/* flush I,D TLBs */
	mov	r1, #BOOT_PGD_PHYS
	mcr	p15, 0, r1, c2, c0, 0	/* load page table pointer */
	mov	r0, #-1
	mcr	p15, 0, r0, c3, c0	/* load domain access register */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x1000		/* I-cache enable */
	orr	r0, r0, #0x003d		/* Write buffer, mmu */
	mcr	p15, 0, r0, c1, c0, 0

	/*
	 * Reload PC register for virutal address.
	 */
	ldr	pc, reload_pc_target	/* Reset pc here */
reload_pc:

	/*
	 * Unmap 0-4M.
	 * Since the first page must be accessible for exception
	 * vector, we have to map it later.
	 */
	mov	r1, #BOOT_PGD_PHYS	/* Set PTE0 address in pgd */
	add	r1, #PAGE_OFFSET
	mov	r0, #0
	str	r0, [r1]
	mcr	p15, 0, r0, c8, c7, 0	/* flush I,D TLBs */

#endif /* !CONFIG_MMU */

	/*
	 * Clear kernel BSS
	 */
	ldr	r1, =__bss
	ldr	r2, =__end
	mov	r0, #0
	cmp	r1, r2
	beq	2f
1:	str	r0, [r1], #4
	cmp	r1, r2
	bls	1b
2:

	/*
	 * Jump to kernel main routine
	 */
	b	main

/*
 * Relocate exception vector
 *
 * void vector_copy(paddr_t dest);
 */
ENTRY(vector_copy)
	ldr	r1, =vector_start
	ldr	r2, =vector_end
copy_loop:
	ldmia	r1!, {r3}
	stmia	r0!, {r3}
	teq	r1, r2
	bne	copy_loop
	mov	pc, lr

#ifdef CONFIG_CACHE
/*
 * Enable cache
 */
ENTRY(cache_init)
	mov	pc, lr
#endif

/*
 * Interrupt entry point
 */
/*
 * Memo: GBA BIOS interrupt handler.
 *
 *	stmfd	sp!, {r0-r3,r12,lr}
 *	mov	r0, #0x4000000
 *	adr	lr, IntRet
 *	ldr	pc, [r0,#-4] @ pc = [0x3007ffc]
 *IntRet:
 *	ldmfd	sp!, {r0-r3,r12,lr}
 *	subs	pc, lr, #4
 */
ENTRY(interrupt_entry)
#ifdef __gba__
	ldmfd	sp!, {r0-r3,r12,lr}	/* Discard GBA BIOS's stack */
#endif
	stmfd	sp, {r0-r4}		/* Save work registers */
	sub	r2, lr, #4		/* r2: pc */
	mrs	r3, spsr		/* r3: cpsr */
	sub	r4, sp, #(4*5)		/* Pointer to saved registers */

	mrs	r0, cpsr		/* Set processor to SVC mode */
	bic	r0, r0, #PSR_MODE
	orr	r0, r0, #PSR_SVC_MODE
	msr	cpsr_c, r0

	mov	r0, sp
	mov	r1, lr
	stmfd	sp!, {r0-r3}		/* Push svc_sp, svc_lr, pc, cpsr */
	ldmfd	r4, {r0-r4}		/* Restore work registers */
	sub	sp, sp, #(4*15)
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */

	ldr	r4, irq_nest_count	/* Increment IRQ nesting level */
	ldr	r5, [r4]		/* r5: Previous nesting level */
	add	r0, r5, #1
	str	r0, [r4]

	mov	r7, sp			/* Save stack */
	ldr	r3, int_stack		/* Adjust stack for IRQ */
	cmp	r5, #0			/* Outermost interrupt? */
	moveq	sp, r3			/* If outermost, switch stack */
	bleq	sched_lock		/* If outermost, lock scheduler */
	bl	interrupt_handler	/* Call main interrupt handler */
	mov	sp, r7			/* Restore stack */

	str	r5, [r4]		/* Restore IRQ nesting level */
	cmp	r5, #0			/* Outermost interrupt? */
	bne	nested_irq

	bl	sched_unlock		/* Try to preempt */
	ldr	r0, [sp, #(4*18)]	/* Get previous mode */
	and	r0, r0, #PSR_MODE
	cmp	r0, #PSR_APP_MODE	/* Return to application mode? */
	bleq	exception_deliver	/* If so, check exception */

nested_irq:
	mov	r0, sp
 	ldr	sp, [r0, #(4*15)]	/* Restore svc_sp */
 	ldr	lr, [r0, #(4*16)]	/* Restore svc_lr */

	mrs	r1, cpsr		/* Set processor to IRQ mode */
	bic	r1, r1, #PSR_MODE
	orr	r1, r1, #PSR_IRQ_MODE
	msr	cpsr_c, r1

 	ldr	lr, [r0, #(4*17)]	/* Restore lr */
 	ldr	r1, [r0, #(4*18)]	/* Restore spsr */
 	msr	spsr_all, r1
 	ldmfd	r0, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	movs	pc, lr			/* Exit, with restoring cpsr */

/*
 * System call entry
 */
	.global syscall_ret
ENTRY(syscall_entry)
#ifdef __gba__
	mov	r5, lr			/* Syscall stub already saved r5 */
	mrs	r12, cpsr		/* Set processor to SVC mode */
	bic	r12, r12, #PSR_MODE
	orr	r12, r12, #PSR_SVC_MODE
	msr	cpsr_c, r12
	mov	lr, r5
#endif
	sub	sp, sp, #(4*19)		/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	add	r5, sp, #(4*19)
	str	r5, [sp, #(4*15)]	/* Push svc_sp */
	str	lr, [sp, #(4*17)]	/* Push pc */
	mrs	r5, spsr		/* Push cpsr */
	str	r5, [sp, #(4*18)]
#ifndef __gba__
	ldr	r4, [lr, #-4]		/* Get SWI number */
	bic	r4, r4, #0xff000000
#endif
	ldr	r5, =nr_syscalls	/* Check SWI number */
	ldr	r5, [r5]
	cmp	r4, r5
	bge	bad_syscall

	ldr	r5,=syscall_table
	ldr	r4, [r5, r4, lsl #2]
	mov	lr, pc
	mov	pc, r4			/* Dispatch functions */
	str	r0, [sp]		/* Set return value to r0 */
	bl	exception_deliver	/* Check exception */
syscall_ret:
	mov	r5, sp
	ldr	r1, [r5, #(4*18)]	/* Restore cpsr */
	msr	spsr_all, r1
	ldr	lr, [r5, #(4*17)]	/* Restore pc (lr) */
	ldr	sp, [r5, #(4*15)]	/* Restore svc_sp */
	ldmfd	r5, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	movs	pc, lr			/* Exit, with restoring cpsr */
bad_syscall:
	mov	r0, #22			/* Set EINVAL to r0 */
	str	r0, [sp, #(4*0)]
	b	syscall_ret

/*
 * Undefined instruction
 */
ENTRY(undefined_entry)
	sub	sp, sp, #(4*19)		/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mov	r0, #TRAP_UNDEFINED
	b	trap_common

/*
 * Prefetch abort
 */
ENTRY(prefetch_entry)
	sub	lr, lr, #8		/* Adjust the lr */
	sub	sp, sp, #(4*19)		/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mov	r0, #TRAP_PREFETCH_ABORT
	b	trap_common

/*
 * Data abort
 */
ENTRY(abort_entry)
	sub	lr, lr, #4		/* Adjust the lr */
	sub	sp, sp, #(4*19)		/* Adjust stack */
	stmia	sp, {r0-r14}^		/* Push r0-r14 */
	nop				/* Instruction gap for stm^ */
	mov	r0, #TRAP_DATA_ABORT
	b	trap_common

/*
 * Common entry for all traps
 * r0 - trap type
 */
ENTRY(trap_common)
	add	r5, sp, #(4*19)
	str	r5, [sp, #(4*15)]	/* Push svc_sp */
	str	lr, [sp, #(4*17)]	/* Push pc */
	mrs	r5, spsr		/* Push cpsr */
	str	r5, [sp, #(4*18)]

	str	r0, [sp, #(4*0)]	/* Set trap type to R0 */
	mov	r0, sp
	bl	trap_handler

	mov	r5, sp
	ldr	r1, [r5, #(4*18)]	/* Restore cpsr */
	msr	spsr_all, r1
	ldr	lr, [r5, #(4*17)]	/* Restore pc (lr) */
	ldr	sp, [r5, #(4*15)]	/* Restore svc_sp */
	ldmfd	r5, {r0-r14}^		/* Restore user mode registers */
	nop				/* Instruction gap for ldm^ */
	movs	pc, lr			/* Exit, with restoring cpsr */

ENTRY(fiq_entry)
	b	fiq_entry		/* Not support... */

/*
 * Switch register context.
 * r0 = previous kern_regs, r1 = next kern_regs
 * Interrupts must be disabled by caller.
 *
 * syntax - void cpu_switch(kern_regs *prev, kern_regs *next)
 *
 * Note: GCC uses r0-r3,r12 as scratch registers
 */
ENTRY(cpu_switch)
	stmia	r0, {r4-r11, sp, lr}	/* Save previous register context */
	ldmia	r1, {r4-r11, sp, pc}	/* Restore next register context */

/*
 * Entry point for kernel thread
 */
ENTRY(kernel_thread_entry)
	mov	r0, r5			/* Set argument */
	mov	pc, r4			/* Jump to kernel thread */


/*
 * Copy data from user to kernel space.
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int umem_copyin(const void *uaddr, void *kaddr, size_t len)
 */
	.global known_fault1
ENTRY(umem_copyin)
 	mov	r12, sp
 	stmdb	sp!, {r4, r11, r12, lr, pc}
 	sub	r11, r12, #4
	cmp	r0, #(UMEM_MAX)
	bhi	umem_fault
 	mov	r12, #0
 	b	2f
1: 	ldrb	r3, [r12, r0]
known_fault1:				/* May be fault here */
 	strb	r3, [r12, r1]
 	add	r12, r12, #1
2: 	subs	r2, r2, #1
 	bcs	1b
	mov	r0, #0			/* Set no error */
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * Copy data to user from kernel space.
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int umem_copyout(const void *kaddr, void *uaddr, size_t len)
 */
	.global known_fault2
ENTRY(umem_copyout)
 	mov	r12, sp
 	stmdb	sp!, {r4, r11, r12, lr, pc}
 	sub	r11, r12, #4
	cmp	r1, #(UMEM_MAX)
	bhi	umem_fault
 	mov	r12, #0
 	b	2f
1: 	ldrb	r3, [r12, r0]
known_fault2:				/* May be fault here */
 	strb	r3, [r12, r1]
 	add	r12, r12, #1
2: 	subs	r2, r2, #1
 	bcs	1b
	mov	r0, #0			/* Set no error */
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * umem_strnlen - Get length of string in user space
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int umem_strnlen(const char *uaddr, size_t maxlen, size_t *len)
 *
 * Note: The returned length value does NOT include the NULL terminator.
 */
	.global known_fault3
ENTRY(umem_strnlen)
 	mov	r12, sp
 	stmdb	sp!, {r4, r11, r12, lr, pc}
	mov	r4, r0
 	sub	r11, r12, #4
	cmp	r0, #(UMEM_MAX)
	bhi	umem_fault
	b	2f
1:	add	r0, r0, #1
2: 	subs	r1, r1, #1
 	bcc	3f
known_fault3:				/* May be fault here */
 	ldrb	r3, [r0]
	cmp	r3, #0
 	bne	1b
3: 	rsb	r0, r4, r0
	str	r0, [r2]
	mov	r0, #0			/* Set no error */
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * Fault entry for user access
 */
ENTRY(umem_fault)
	mov	r0, #14			/* Set EFAULT as return */
 	ldmia	sp, {r4, r11, sp, pc}

/*
 * Interrupt nest counter.
 *
 * This counter is incremented in the entry of interrupt handler
 * to switch the interrupt stack. Since all interrupt handlers
 * share same one interrupt stack, each handler must pay attention
 * to the stack overflow.
 */
	.section ".bss"
irq_nesting:
	.long	0
	.end
