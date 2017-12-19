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
 * trap.c - called from the trap handler when a processor trap occurs.
 */

#include <kernel.h>
#include <exception.h>
#include <thread.h>
#include <task.h>
#include <cpu.h>
#include <locore.h>
#include <cpufunc.h>

#ifdef DEBUG
static void trap_dump(struct cpu_regs *);

static char *const trap_name[] = {
	"Divide error",		/*  0 */
	"Debug trap",		/*  1 */
	"NMI",			/*  2 */
	"Breakpoint",		/*  3 */
	"Overflow",		/*  4 */
	"Bounds check",		/*  5 */
	"Invalid opecode",	/*  6 */
	"Device not available",	/*  7 */
	"Double fault",		/*  8 */
	"Coprocessor overrun",	/*  9 */
	"Invalid TSS",		/* 10 */
	"Segment not present",	/* 11 */
	"Stack bounds",		/* 12 */
	"General Protection",	/* 13 */
	"Page fault",		/* 14 */
	"Reserved",		/* 15 */
	"Coprocessor error",	/* 16 */
	"Alignment check",	/* 17 */
	"Cache flush denied"	/* 18 */
};
#define MAXTRAP (sizeof(trap_name) / sizeof(void *) - 1)
#endif	/* DEBUG */

/*
 * Trap/exception mapping table.
 * i386 trap code is translated to the architecture
 * independent exception code.
 */
static const int exception_map[] = {
	SIGFPE,		/*  0: Divide error */
	SIGTRAP,	/*  1: Debug trap */
	SIGILL,		/*  2: NMI */
	SIGTRAP,	/*  3: Breakpoint */
	SIGFPE,		/*  4: Overflow */
	SIGILL,		/*  5: Bounds check */
	SIGILL,		/*  6: Invalid opecode */
	SIGFPE,		/*  7: Device not available */
	SIGILL,		/*  8: Double fault */
	SIGFPE,		/*  9: Coprocessor overrun */
	SIGSEGV,	/* 10: Invalid TSS */
	SIGSEGV,	/* 11: Segment not present */
	SIGSEGV,	/* 12: Stack bounds */
	SIGILL,		/* 13: General Protection fault */
	SIGSEGV,	/* 14: Page fault */
	SIGILL,		/* 15: Reserved */
	SIGFPE,		/* 16: Coprocessor error */
	SIGILL,		/* 17: Alignment check */
	SIGILL,		/* 18: Cache flush denied */
};

/*
 * Trap handler
 * Invoke the exception handler if it is needed.
 */
void
trap_handler(struct cpu_regs *regs)
{
	u_long trap_no = regs->trap_no;
#ifdef DEBUG
	task_t self = cur_task();
#endif

	if (trap_no > 18)
		panic("Unknown trap");
	else if (trap_no == 2)
		panic("NMI");

	/*
	 * Check whether this trap is kernel page fault caused
	 * by known routine to access user space like umem_copyin().
	 * If so, we change the return address of this exception.
	 */
	if (trap_no == 14 && regs->cs == KERNEL_CS &&
	    (regs->eip == (uint32_t)known_fault1 ||
	     regs->eip == (uint32_t)known_fault2 ||
	     regs->eip == (uint32_t)known_fault3)) {
		DPRINTF(("\n*** Detect Fault! address=%x task=%s ***\n",
			 get_cr2(),
			 self->name != NULL ? self->name : "no name", self));
		regs->eip = (uint32_t)umem_fault;
		return;
	}
#ifdef DEBUG
	printf("============================\n");
	printf("Trap %x: %s\n", (u_int)trap_no, trap_name[trap_no]);
	if (trap_no == 14)
		printf(" Fault address=%x\n", get_cr2());
	printf("============================\n");
	trap_dump(regs);
	if (regs->cs == KERNEL_CS) {
		interrupt_mask(0);
		interrupt_enable();
		for (;;) ;
	}
#endif
	if (regs->cs == KERNEL_CS)
		panic("Kernel exception");

	exception_mark(exception_map[trap_no]);
	exception_deliver();
}

#ifdef DEBUG
static void
trap_dump(struct cpu_regs *r)
{
	task_t self = cur_task();
	uint32_t ss, esp, *fp;
	u_int i;

	if (r->cs & 3) {
		ss = r->ss;
		esp = r->esp;
	} else {
		ss = r->ds;
		esp = (uint32_t)r;
	}
	printf("Trap frame %x error %x\n", r, r->err_code);
	printf(" eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x\n",
	       r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi);
	printf(" eip %08x esp %08x ebp %08x eflags %08x\n",
	       r->eip, esp, r->ebp, r->eflags);
	printf(" cs  %08x ss  %08x ds  %08x es  %08x esp0 %08x\n",
	       r->cs, ss, r->ds, r->es, tss_get());

	if (irq_level > 0)
		printf(" >> trap in isr (irq_level=%d)\n", irq_level);

	printf(" >> interrupt is %s\n",
	       (get_eflags() & EFL_IF) ? "enabled" : "disabled");

	printf(" >> task=%s (id:%x)\n",
	       self->name != NULL ? self->name : "no name", self);

	if (r->cs == KERNEL_CS) {
		printf("Stack trace:\n");
		fp = (uint32_t *)r->ebp;
		for (i = 0; i < 8; i++) {
			if (fp == 0)
				break;
			fp = (uint32_t *)(*fp);	/* XXX: may cause fault */
			if (!(*(fp + 1) && *fp))
				break;
			printf(" %08x\n", *(fp + 1));
		}
	}
}
#endif /* DEBUG */
