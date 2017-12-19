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

#include <machine/asm.h>


ENTRY(breakpoint)
	int	$3
	ret

ENTRY(outb)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	outb	%al, %dx
	ret

ENTRY(outw)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	outw	%ax, %dx
	ret

ENTRY(outl)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	outl	%eax, %dx
	ret

ENTRY(inb)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inb	%dx, %al
	ret

ENTRY(inw)
	movl	4(%esp), %edx
	xorl	%eax, %eax
	inw	%dx, %ax
	ret

ENTRY(inl)
	movl	4(%esp), %edx
	inl	%dx, %eax
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

ENTRY(rdmsr)
	movl	4(%esp), %ecx
	rdmsr
	movl	8(%esp), %ecx
	movl	%eax, (%ecx)
	movl	12(%esp), %ecx
	movl	%edx, (%ecx)
	ret

ENTRY(wrmsr)
	movl	4(%esp), %ecx
	movl	8(%esp), %eax
	movl	12(%esp), %edx
	wrmsr
	ret

ENTRY(cpuid)
	pushl	%ebx
	pushl	%edi
	movl	12(%esp), %eax
	movl	16(%esp), %edi
	cpuid
	movl	%eax, 0(%edi)
	movl	%ebx, 4(%edi)
	movl	%ecx, 8(%edi)
	movl	%edx, 12(%edi)
	popl	%edi
	popl	%ebx
	ret
