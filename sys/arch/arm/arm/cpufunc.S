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
 * cpufunc.S - ARM specific CPU functions
 */

#include <conf/config.h>
#include <machine/asm.h>
#include <syspage.h>
#include <cpu.h>

	.section ".text","ax"
	.code 32

ENTRY(cpu_idle)
#if 0
	mcr	p15, 0, r0, c7, c0, 4	/* Wait for interrupt */
#endif
	mov	pc, lr

ENTRY(interrupt_enable)
	mrs	r0, cpsr
	bic	r0, r0, #0xc0
	msr	cpsr_c, r0
	mov	pc, lr

ENTRY(interrupt_disable)
	mrs	r0, cpsr
	orr	r0, r0, #0xc0
	msr	cpsr_c, r0
	mov	pc, lr

ENTRY(interrupt_save)
	mrs	r1, cpsr
	str	r1, [r0]
	mov	pc, lr

ENTRY(interrupt_restore)
	msr	cpsr_c, r0
	mov	pc, lr

/*
 * Fault information
 */
ENTRY(get_faultstatus)
	mrc	p15, 0, r0, c5, c0, 0
	mov	pc, lr

ENTRY(get_faultaddress)
	mrc	p15, 0, r0, c6, c0, 0
	mov	pc, lr


#ifdef CONFIG_MMU
/*
 * Get TTB - Translation Table Base register
 */
ENTRY(get_ttb)
	mrc	p15, 0, r0, c2, c0, 0
	mov	pc, lr

/*
 * Set TTB
 */
ENTRY(set_ttb)
	mcr	p15, 0, r0, c2, c0, 0	/* load new TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	nop
	nop
	nop
	mov	pc, lr

/*
 * Flush TLB
 */
ENTRY(flush_tlb)
	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	mov	pc, lr

/*
 * Switch TTB for context switch
 */
ENTRY(switch_ttb)
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache */
	mcr	p15, 0, r0, c7, c6, 0	/* flush D cache */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mcr	p15, 0, r0, c2, c0, 0	/* set the new TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* and flush the I+D tlbs */
	nop
	nop
	nop
	mov	pc, lr

#endif /* !CONFIG_MMU */

/*
 * Flush all cache
 */
ENTRY(flush_cache)
	mcr	p15, 0, r0, c7, c5, 0	/* flush I cache */
	mcr	p15, 0, r0, c7, c6, 0	/* flush D cache */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr


	.end
