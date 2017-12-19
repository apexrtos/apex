/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * context.c - context management routines
 */

/*
 * The context consists of kernel/user mode registers, and kernel
 * stack. The user mode registers are always saved to the kernel
 * stack when processor enters kernel mode by H/W or S/W events.
 *
 * The user mode registers are located in the interrupt/trap
 * frame at the top of the kernel stack. Before the control
 * returns to user mode next time, these register value will be
 * restored automatically.
 *
 * All thread owns its context to keep its execution state. The
 * scheduler will switch the context to change an active thread.
 */

#include <kernel.h>
#include <cpu.h>
#include <locore.h>

/*
 * Set user mode registers into the specific context.
 *
 * Note: When user mode program counter is set, all register
 * values except a stack pointer are reset to default value.
 */
void
context_set(context_t ctx, int type, vaddr_t val)
{
	struct kern_regs *k;
	struct cpu_regs *u;
	uint32_t *argp;

	k = &ctx->kregs;

	switch (type) {
	case CTX_KSTACK:
		/* Set kernel mode stack pointer */
		ctx->uregs = (struct cpu_regs *)
				((uint32_t)val - sizeof(struct cpu_regs));
		ctx->esp0 = (uint32_t)val;

		k->eip = (uint32_t)&syscall_ret;
		k->esp = (uint32_t)ctx->uregs - sizeof(uint32_t);

		/* Reset minimum user mode registers */
		u = ctx->uregs;
		u->eax = 0;
		u->eflags = (uint32_t)(EFL_IF | EFL_IOPL_KERN);
		break;

	case CTX_KENTRY:
		/* Kernel mode program counter */
		k->eip = (uint32_t)val;
		break;

	case CTX_KARG:
		/* Kernel mode argument */
		argp = (uint32_t *)(k->esp + sizeof(uint32_t) * 2);
		*argp = (uint32_t)val;
		break;

	case CTX_USTACK:
		/* User mode stack pointer */
		u = ctx->uregs;
		u->esp = (uint32_t)val;
		u->ss = (uint32_t)(USER_DS | 3); /* fail safe */
		break;

	case CTX_UENTRY:
		/* User mode program counter */
		u = ctx->uregs;
		u->eip = (uint32_t)val;
		u->cs = (uint32_t)(USER_CS | 3);
		u->ds = u->es = (uint32_t)(USER_DS | 3);
		u->eflags = (uint32_t)(EFL_IF | EFL_IOPL_KERN);
		u->eax = u->ebx = u->ecx = u->edx =
			u->edi = u->esi = u->ebp = 0x12345678;
		break;

	case CTX_UARG:
		/* User mode argument */
		u = ctx->uregs;
		argp = (uint32_t *)(u->esp + sizeof(uint32_t));
		umem_copyout(&val, argp, sizeof(uint32_t));
		break;

	default:
		/* invalid */
		break;
	}

}

/*
 * Switch to new context
 *
 * Kernel mode registers and kernel stack pointer are switched to
 * the next context.
 *
 * We don't use x86 task switch mechanism to minimize the context
 * space. The system has only one TSS(task state segment), and
 * the context switching is done by changing the register value
 * in this TSS. Processor will reload them automatically when it
 * enters to the kernel mode in next time.
 *
 * It is assumed all interrupts are disabled by caller.
 *
 * TODO: FPU context is not switched as of now.
 */
void
context_switch(context_t prev, context_t next)
{
	/* Set kernel stack pointer in TSS (esp0). */
	tss_set((uint32_t)next->esp0);

	/* Save the previous context, and restore the next context */
	cpu_switch(&prev->kregs, &next->kregs);
}

/*
 * Save user mode context to handle exceptions.
 *
 * Copy current user mode registers in the kernel stack to the
 * user mode stack. The user stack pointer is adjusted for this
 * area. So that the exception handler can get the register
 * state of the target thread.
 *
 * It builds arguments for the exception handler in the following
 * format.
 *
 *   void exception_handler(int exc, void *regs);
 */
void
context_save(context_t ctx)
{
	struct cpu_regs *cur, *sav;

	/* Copy current register context into user mode stack */
	cur = ctx->uregs;
	sav = (struct cpu_regs *)(cur->esp - sizeof(struct cpu_regs));
	*sav = *cur;

	ctx->saved_regs = sav;

	/* Adjust stack pointer */
	cur->esp = (uint32_t)(sav - (sizeof(uint32_t) * 2));
}

/*
 * Restore register context to return from the exception handler.
 */
void
context_restore(context_t ctx)
{
	struct cpu_regs *cur;

	/* Restore user mode context */
	cur = ctx->uregs;
	*cur = *ctx->saved_regs;

	/* Correct some registers for fail safe */
	cur->cs = (uint32_t)(USER_CS | 3);
	cur->ss = cur->ds = cur->es = (uint32_t)(USER_DS | 3);
	cur->eflags |= EFL_IF;

	ASSERT(cur->eip && user_area(cur->eip));
	ASSERT(cur->esp && user_area(cur->esp));
}
