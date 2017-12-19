/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
#include <mmu.h>	/* for UMEM_MAX */
#include <cpu.h>

/*
 * Macro to save/restore registers
 *
 * This macro builds the trap frame by pushing registers.
 * If you change the push order of these macro, you must change the
 * trap frame structure in arch.h. In addition, the system call stub
 * will depend on this register format.
 */
#define SAVE_ALL \
	cld; \
	pushl	%es; \
	pushl	%ds; \
	pushl	%eax; \
	pushl	%ebp; \
	pushl	%edi; \
	pushl	%esi; \
	pushl	%edx; \
	pushl	%ecx; \
	pushl	%ebx;

#define RESTORE_ALL \
	popl	%ebx; \
	popl	%ecx; \
	popl	%edx; \
	popl	%esi; \
	popl	%edi; \
	popl	%ebp; \
	popl	%eax; \
	popl	%ds; \
	popl	%es;

#define SETUP_SEG \
	movl	$(KERNEL_DS), %edx; \
	movl	%edx, %ds; \
	movl	%edx, %es;

	.section ".text"

/*
 * Kernel start point
 *
 * The kernel assumes that the following state is already set by
 * the kernel loader.
 * - CPU is in protected mode
 * - Segment registers are set as 32-bit flat segment
 * - A20 line is enabled for 32-bit addressing
 * - Paging is turned off
 * - The boot information is loaded to address 1000h-1fffh
 */
/*
 * Note: The linker will generate an address for kernel_start as 0x80010000.
 * But, the loader will load the kernel to 0x10000 (physical address).
 * So, the linear address pointer can not be used until paging is enabled.
 */
ENTRY(kernel_start)
	cli				/* Disable interrupt */
	cld

#ifdef CONFIG_MMU
	/*
	 * Initialize page table.
	 * The physical address 0-4M is mapped on virtual address 2G.
	 */
	movl	$(BOOT_PGD_PHYS), %edi	/* Setup kernel page directory */
	xorl	%eax, %eax		/* Invalidate all address */
	movl	$0x1000, %ecx
	rep
	stosb
	movl	$(BOOT_PGD_PHYS+0x800), %edi
	movl	$(BOOT_PTE0_PHYS+0x07), (%edi)
	movl	$1024, %ecx		/* Fill boot page table entry */
	movl	$(BOOT_PTE0_PHYS), %edi
	movl	$0007, %eax
fill_pte0:
	stosl
	add	$0x1000, %eax
	loop	fill_pte0

	/*
	 * Enable paging.
	 * The physical address 0-4M is temporarily mapped to virtial
	 * address 0-4M. This is needed to enable paging.
	 */
	movl	$(BOOT_PGD_PHYS), %edi	/* Map address 0-4M */
	movl	$(BOOT_PTE0_PHYS+0x07), (%edi)
	movl	$(BOOT_PGD_PHYS), %eax	/* Set page directory pointer */
	movl	%eax, %cr3
	movl	%cr0, %eax		/* Enable paging bit */
	orl	$0x80000000, %eax
	movl	%eax, %cr0
	jmp	pipeline_flush		/* Flush processor pipeline */
pipeline_flush:
	movl	$cs_reset, %eax
	jmp	*%eax
cs_reset:
	movl	$(BOOT_PGD), %edi	/* Unmap 0-4M */
	movl	$0, (%edi)
	movl	%cr3, %eax
	movl	%eax, %cr3

#endif /* CONFIG_MMU */

	/*
	 * Clear kernel BSS
	 */
	xorl	%eax, %eax
	movl	$__bss, %edi
	movl	$__end, %ecx
	subl	%edi, %ecx
	rep
	stosb

	/*
	 * Setup boot stack
	 */
	movl	$(BOOTSTACK_BASE), %edi
	movl	$(BOOTSTACK_SIZE), %ecx
	rep
	stosb
	movl	$(BOOTSTACK_TOP), %esp
	movl	%esp, %ebp

	/*
	 * Call kernel main routine
	 */
	call	main
	/* NOTREACHED */
	cli
	hlt

/*
 * Common entry for all interrupts
 * Setup interrupt stack for outermost interrupt.
 * The code should be written to prevent the stack overflow
 * by continuous interrupt as much as it can.
 */
ENTRY(interrupt_common)
	SAVE_ALL
	SETUP_SEG
	incl	irq_nesting		/* Increment nesting level */
	cmpl	$1, irq_nesting		/* Outermost interrupt ? */
	jne	nested_irq
	mov	%esp, %ebp		/* Save current stack */
	movl	$(INTSTACK_TOP), %esp	/* Switch stack */
	call	sched_lock		/* Lock scheduler */
	pushl	%ebp			/* Push trap frame */
	call	interrupt_handler	/* Process interrupt */
	movl	%ebp, %esp		/* Restore original stack */
	decl	irq_nesting
	call	sched_unlock		/* Try to preempt */
	testl	$3, 0x30(%esp)		/* Return to kernel mode ? */
	jz	interrupt_ret		/* Skip exception if kernel mode */
	call	exception_deliver	/* Check exception */
interrupt_ret:
	RESTORE_ALL
	addl	$8, %esp
	iret
nested_irq:
	push	%esp			/* Push trap frame */
	call	interrupt_handler	/* Process interrupt */
	addl	$4, %esp
	decl	irq_nesting
	jmp	interrupt_ret

/*
 * Macro to build interrupt entry
 */
#define INTR_ENTRY(irq) \
ENTRY(intr_##irq) \
	pushl	$0; \
	pushl	$(irq); \
	jmp	interrupt_common

INTR_ENTRY(0)
INTR_ENTRY(1)
INTR_ENTRY(2)
INTR_ENTRY(3)
INTR_ENTRY(4)
INTR_ENTRY(5)
INTR_ENTRY(6)
INTR_ENTRY(7)
INTR_ENTRY(8)
INTR_ENTRY(9)
INTR_ENTRY(10)
INTR_ENTRY(11)
INTR_ENTRY(12)
INTR_ENTRY(13)
INTR_ENTRY(14)
INTR_ENTRY(15)

/*
 * Common entry for all traps
 * New thread will start from trap_ret.
 */
ENTRY(trap_common)
	SAVE_ALL
	SETUP_SEG
	pushl	%esp
	call	trap_handler
	addl	$4, %esp
trap_ret:
	RESTORE_ALL
	addl	$8, %esp
	iret

/*
 * Default trap entry
 */
ENTRY(trap_default)
	pushl	$0
	pushl	$(INVALID_INT)
	jmp	trap_common

/*
 * Macro to build trap entry
 * Some trap will push the error code into stack.
 */
#define TRAP_ENTRY(id) \
ENTRY(trap_##id) \
	pushl	$0; \
	pushl	$(id); \
	jmp	trap_common;

#define TRAP_ERR_ENTRY(id) \
ENTRY(trap_##id) \
	pushl	$(id); \
	jmp	trap_common;

TRAP_ENTRY    ( 0)		/* Divide error */
TRAP_ENTRY    ( 1)		/* Debug trap */
TRAP_ENTRY    ( 2)		/* NMI */
TRAP_ENTRY    ( 3)		/* Breakpoint */
TRAP_ENTRY    ( 4)		/* Overflow */
TRAP_ENTRY    ( 5)		/* Bounds check */
TRAP_ENTRY    ( 6)		/* Invalid opecode */
TRAP_ENTRY    ( 7)		/* Device not available */
TRAP_ERR_ENTRY( 8)		/* Double fault */
TRAP_ERR_ENTRY( 9)		/* Coprocessor overrun */
TRAP_ERR_ENTRY(10)		/* Invalid TSS */
TRAP_ERR_ENTRY(11)		/* Segment not present */
TRAP_ERR_ENTRY(12)		/* Stack bounds */
TRAP_ERR_ENTRY(13)		/* General Protection */
TRAP_ERR_ENTRY(14)		/* Page fault */
TRAP_ENTRY    (15)		/* (reserved) */
TRAP_ENTRY    (16)		/* Coprocessor error */
TRAP_ERR_ENTRY(17)		/* Alignment check */
TRAP_ERR_ENTRY(18)		/* Cache flush denied */

/*
 * System call entry
 */
	.global syscall_ret
ENTRY(syscall_entry)
	pushl	$0			/* Dummy for error code */
	pushl	$(SYSCALL_INT)		/* Trap number */
	SAVE_ALL
	SETUP_SEG
	cmpl	nr_syscalls, %eax
	jae	bad_syscall
	call	*syscall_table(,%eax,4)	/* Call function */
	movl	%eax, 0x18(%esp)	/* Set return value to eax */
	call	exception_deliver	/* Check exception */
syscall_ret:
	RESTORE_ALL
	addl	$8, %esp		/* Discard err/trap no */
	iret
bad_syscall:
	movl	$22, 0x18(%esp)		/* Set EINVAL error to eax */
	jmp	syscall_ret


/*
 * Switch register context.
 * Interrupts must be disabled by caller.
 *
 * syntax - void cpu_switch(kern_regs *prev, kern_regs *next)
 *
 * Note: GCC assumes ebx,ebp,edi,esi registers are not changed in each routine.
 */
ENTRY(cpu_switch)
	movl	4(%esp), %ecx		/* Point ecx to previous registers */
	movl	(%esp), %eax		/* Get return address */
	movl	%eax, 0(%ecx)		/* Save it as eip */
	movl	%ebx, 4(%ecx)		/* Save ebx */
	movl	%edi, 8(%ecx)		/* Save edi */
	movl	%esi, 12(%ecx)		/* Save esi */
	movl	%ebp, 16(%ecx)		/* Save ebp */
	movl	%esp, 20(%ecx)		/* Save esp */
	movl	8(%esp), %ecx		/* Point ecx to next registers */
	movl	4(%ecx), %ebx		/* Restore ebx */
	movl	8(%ecx), %edi		/* Restore edi */
	movl	12(%ecx), %esi		/* Restore esp */
	movl	16(%ecx), %ebp		/* Restore ebp */
	movl	20(%ecx), %esp		/* Restore esp */
	movl	0(%ecx), %eax		/* Get eip */
	movl	%eax, (%esp)		/* Restore it as return address */
	ret

/*
 * Copy data from user to kernel space.
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int umem_copyin(const void *uaddr, void *kaddr, size_t len)
 */
	.global known_fault1
ENTRY(umem_copyin)
	pushl	%esi
	pushl	%edi
	pushl	$14			/* Set EFAULT as default return */

	movl	16(%esp), %esi
	movl	20(%esp), %edi
	movl	24(%esp), %ecx

	movl	%esi, %edx		/* Check if valid user address */
	addl	%ecx, %edx
	jc	umem_fault
	cmpl	$(UMEM_MAX), %edx
	jae	umem_fault
	cld
known_fault1:				/* May be fault here */
	rep
	movsb

	popl	%eax
	xorl	%eax, %eax		/* Set no error */
	popl	%edi
	popl	%esi
	ret

/*
 * Copy data to user from kernel space.
 * Returns 0 on success, or EFAULT on page fault.
 *
 *  syntax - int umem_copyout(const void *kaddr, void *uaddr, size_t len)
 */
	.global known_fault2
ENTRY(umem_copyout)
	pushl	%esi
	pushl	%edi
	pushl	$14			/* Set EFAULT as default return */

	movl	16(%esp), %esi
	movl	20(%esp), %edi
	movl	24(%esp), %ecx

	movl	%edi, %edx
	addl	%ecx, %edx
	jc	umem_fault
	cmpl	$(UMEM_MAX), %edx
	jae	umem_fault
	cld
known_fault2:
	rep
	movsb

	popl	%eax
	xorl	%eax, %eax		/* Set no error */
	popl	%edi
	popl	%esi
	ret

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
	pushl	%esi
	pushl	%edi
	pushl	$14			/* Set EFAULT as default return */

	movl	16(%esp), %edi
	movl	20(%esp), %ecx
	movl	24(%esp), %esi

	movl	%edi, %edx
	cmpl	$(UMEM_MAX), %edx
	jae	umem_fault
	movl	%ecx, %edx
	cld
	xor	%eax, %eax
known_fault3:
	repne
	scasb
	subl	%ecx, %edx
	decl	%edx			/* Adjust for terminator */
	movl	%edx, (%esi)

	popl	%eax
	xorl	%eax, %eax		/* Set no error */
	popl	%edi
	popl	%esi
	ret

/*
 * Fault entry for user access
 */
ENTRY(umem_fault)
	popl	%eax			/* Get return value from stack */
	popl	%edi
	popl	%esi
	ret

/*
 * Reset cpu
 * Use triple fault
 */
ENTRY(cpu_reset)
	cli
	movl	$null_idt, %eax		/* Reset by triple fault */
	lidt	(%eax)
	int	$3
	hlt

	.align 4
null_idt:
	.word	0
	.long	0

/*
 * Pointer to boot infomation
 */
	.section ".rodata"
.global	bootinfo
bootinfo:
	.long	BOOTINFO_BASE

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
