/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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
 * cpufunc.S - Functions to provide access to i386 specific instructions.
 */

#include <machine/asm.h>
#include <cpu.h>

	.section ".text"

ENTRY(flush_tlb)
	movl	%cr3, %eax
	movl	%eax, %cr3
	ret

ENTRY(flush_cache)
	wbinvd
	ret

ENTRY(ltr)
	movl	4(%esp), %eax
	ltr	%ax
	ret

ENTRY(lgdt)
	movl	4(%esp), %eax
	lgdt	(%eax)
	jmp	1f			/* Flush the prefetch queue */
	nop
1:
	movl	$(KERNEL_DS), %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss
	movl	$(KERNEL_CS), %eax
	pushl	%eax
	pushl	$2f
	lret
2:
	ret

ENTRY(lidt)
	movl	4(%esp), %eax
	lidt	(%eax)
	ret

ENTRY(get_eflags)
	pushfl
	popl	%eax
	ret

ENTRY(set_eflags)
	movl	4(%esp), %eax
	pushl	%eax
	popfl
	ret

ENTRY(set_cr0)
	movl	4(%esp), %eax
	movl	%eax, %cr0
	ret

ENTRY(get_cr0)
	movl	%cr0, %eax
	ret

ENTRY(get_cr2)
	movl	%cr2, %eax
	ret

ENTRY(set_cr3)
	movl	4(%esp), %eax
	movl	%eax, %cr3
	ret

ENTRY(get_cr3)
	movl	%cr3, %eax
	ret

ENTRY(outb)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	outb	%al, %dx
	ret

ENTRY(inb)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inb	%dx, %al
	ret

ENTRY(outb_p)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	outb	%al, %dx
	outb	%al, $0x80
	ret

ENTRY(inb_p)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inb	%dx, %al
	outb	%al, $0x80
	ret

ENTRY(cpu_idle)
	sti
	hlt
	ret

ENTRY(interrupt_disable)
	cli
	ret

ENTRY(interrupt_enable)
	sti
	ret

ENTRY(breakpoint)
	int	$3
	ret
