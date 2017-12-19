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
 * interrupt.c - interrupt handling routines for intel 8259 chip
 */

#include <kernel.h>
#include <irq.h>
#include <cpu.h>
#include <locore.h>
#include <cpufunc.h>

/* I/O address for master/slave programmable interrupt controller */
#define PIC_M           0x20
#define PIC_S           0xa0

/* Edge/level control register */
#define ELCR            0x4d0

/*
 * Interrupt priority level
 *
 * Each interrupt has its logical priority level, with 0 being
 * the highest priority. While some ISR is running, all lower
 * priority interrupts are masked off.
 */
volatile int irq_level;

/*
 * Interrupt mapping table
 */
static int ipl_table[NIRQS];		/* Vector -> level */
static u_int mask_table[NIPLS];		/* Level -> mask */

/*
 * Set mask for current ipl
 */
static void
update_mask(void)
{
	u_int mask = mask_table[irq_level];

	outb(mask & 0xff, PIC_M + 1);
	outb(mask >> 8, PIC_S + 1);
}

/*
 * Unmask interrupt in PIC for specified irq.
 * The interrupt mask table is also updated.
 * Assumed CPU interrupt is disabled in caller.
 */
void
interrupt_unmask(int vector, int level)
{
	int i;
	u_int unmask = (u_int)~(1 << vector);

	ipl_table[vector] = level;
	/*
	 * Unmask target interrupt for all
	 * lower interrupt levels.
	 */
	for (i = 0; i < level; i++)
		mask_table[i] &= unmask;
	update_mask();
}

/*
 * Mask interrupt in PIC for specified irq.
 * Interrupt must be disabled when this routine is called.
 */
void
interrupt_mask(int vector)
{
	int i, level;
	u_int mask = (u_int)(1 << vector);

	level = ipl_table[vector];
	for (i = 0; i < level; i++)
		mask_table[i] |= mask;
	ipl_table[vector] = IPL_NONE;
	update_mask();
}

/*
 * Setup interrupt mode.
 * Select whether an interrupt trigger is edge or level.
 */
void
interrupt_setup(int vector, int mode)
{
	int port;
	u_int bit;
	u_char val;

	port = vector < 8 ? ELCR : ELCR + 1;
	bit = (u_int)(1 << (vector & 7));

	val = inb(port);
	if (mode == IMODE_LEVEL)
		val |= bit;
	else
		val &= ~bit;
	outb(val, port);
}

/*
 * Common interrupt handler.
 *
 * This routine is called from the low level interrupt routine
 * written in assemble code. The interrupt flag is automatically
 * disabled by h/w in CPU when the interrupt is occurred. The
 * target interrupt will be masked in ICU while the irq handler
 * is called.
 */
void
interrupt_handler(struct cpu_regs *regs)
{
	int vector = (int)regs->trap_no;
	int old_ipl, new_ipl;

	/* Adjust interrupt level */
	old_ipl = irq_level;
	new_ipl = ipl_table[vector];
	if (new_ipl > old_ipl)		/* Ignore spurious interrupt */
		irq_level = new_ipl;
	update_mask();

	/* Send acknowledge to PIC for specified irq */
	if (vector & 8)			/* Slave ? */
		outb(0x20, PIC_S);	/* Non specific EOI to slave */
	outb(0x20, PIC_M);		/* Non specific EOI to master */

	/* Dispatch interrupt */
	interrupt_enable();
	irq_handler(vector);
	interrupt_disable();

	/* Restore interrupt level */
	irq_level = old_ipl;
	update_mask();
}

void interrupt_save(int *sts)
{
	*sts = (int)(get_eflags() & EFL_IF);
}

void interrupt_restore(int sts)
{
	set_eflags((get_eflags() & ~EFL_IF) | sts);
}

/*
 * Initialize 8259 interrupt controllers.
 * All interrupts will be masked off in ICU.
 */
void
interrupt_init(void)
{
	int i;

	irq_level = IPL_NONE;

	for (i = 0; i < NIRQS; i++)
		ipl_table[i] = IPL_NONE;

	for (i = 0; i < NIPLS; i++)
		mask_table[i] = 0xfffb;

	outb_p(0x11, PIC_M);		/* Start initialization edge, master */
	outb_p(0x20, PIC_M + 1);	/* Set h/w vector = 0x20 */
	outb_p(0x04, PIC_M + 1);	/* Chain to slave (IRQ2) */
	outb_p(0x01, PIC_M + 1);	/* 8086 mode */

	outb_p(0x11, PIC_S);		/* Start initialization edge, master */
	outb_p(0x28, PIC_S + 1);	/* Set h/w vector = 0x28 */
	outb_p(0x02, PIC_S + 1);	/* Slave (cascade) */
	outb_p(0x01, PIC_S + 1);	/* 8086 mode */

	outb(0xff, PIC_S + 1);		/* Mask all */
	outb(0xfb, PIC_M + 1);		/* Mask all except IRQ2 (cascade) */
}
