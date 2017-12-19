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
 * interrupt.c - interrupt handling routines for GBA
 */

#include <kernel.h>
#include <irq.h>
#include <locore.h>
#include <cpu.h>

/* Interrupt hook vector */
#define IRQ_VECTOR	*(uint32_t *)0x3007ffc

/* Registers for interrupt control unit - enable/flag/master */
#define ICU_IE		(*(volatile uint16_t *)0x4000200)
#define ICU_IF		(*(volatile uint16_t *)0x4000202)
#define ICU_IME		(*(volatile uint16_t *)0x4000208)

/* ICU_IE */
#define IRQ_VALID	0x3fff

/* ICU_IME */
#define IRQ_OFF		0
#define IRQ_ON		1

/*
 * Interrupt priority level
 *
 * Each interrupt has its logical priority level, with 0 being
 * the lowest priority. While some ISR is running, all lower
 * priority interrupts are masked off.
 */
volatile int irq_level;

/*
 * Interrupt mapping table
 */
static int ipl_table[NIRQS];		/* Vector -> level */
static uint16_t mask_table[NIPLS];	/* Level -> mask */

/*
 * Set mask for current ipl
 */
#define update_mask()	ICU_IE = mask_table[irq_level]

/*
 * Unmask interrupt in PIC for specified irq.
 * The interrupt mask table is also updated.
 * Assumes CPU interrupt is disabled in caller.
 */
void
interrupt_unmask(int vector, int level)
{
	int i;
	uint16_t unmask = (uint16_t)1 << vector;

	/* Save level mapping */
	ipl_table[vector] = level;

	/*
	 * Unmask target interrupt for all
	 * lower interrupt levels.
	 */
	for (i = 0; i < level; i++)
		mask_table[i] |= unmask;
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
	u_int mask = (uint16_t)~(1 << vector);

	level = ipl_table[vector];
	for (i = 0; i < level; i++)
		mask_table[i] &= mask;
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
	/* nop */
}

/*
 * Dispatch interrupt
 */
void
interrupt_dispatch(int vector)
{
	int old_ipl;

	/* Save & update interrupt level */
	old_ipl = irq_level;
	irq_level = ipl_table[vector];
	update_mask();

	/* Send acknowledge to ICU for this irq */
	ICU_IF = (uint16_t)(1 << vector);

	/* Allow another interrupt that has higher priority */
	interrupt_enable();

	/* Dispatch interrupt */
	irq_handler(vector);

	interrupt_disable();

	/* Restore interrupt level */
	irq_level = old_ipl;
	update_mask();
}

/*
 * Common interrupt handler.
 */
void
interrupt_handler(void)
{
	uint16_t bits;
	int vector;

	bits = ICU_IF;
retry:
	for (vector = 0; vector < NIRQS; vector++) {
		if (bits & (uint16_t)(1 << vector))
			break;
	}
	if (vector == NIRQS)
		goto out;

	interrupt_dispatch(vector);

	/*
	 * Multiple interrupts can be fired in case of GBA.
	 * So, we have to check the interrupt status, again.
	 */
	bits = ICU_IF;
	if (bits & IRQ_VALID)
		goto retry;
out:
	return;
}

/*
 * Initialize interrupt controllers.
 * All interrupts will be masked off.
 */
void
interrupt_init(void)
{
	int i;

	irq_level = IPL_NONE;

	for (i = 0; i < NIRQS; i++)
		ipl_table[i] = IPL_NONE;

	for (i = 0; i < NIPLS; i++)
		mask_table[i] = 0;

	ICU_IME = IRQ_OFF;

	/*
	 * Since GBA has its own interrupt vector in ROM area,
	 * we can not modify it. Instead, the GBA BIOS will
	 * call the user's interrupt hook routine placed in
	 * the address in 0x3007ffc.
	 */
	IRQ_VECTOR = (uint32_t)interrupt_entry; /* Interrupt hook address */
	ICU_IE = 0;			/* Mask all interrupts */
	ICU_IME = IRQ_ON;
}
