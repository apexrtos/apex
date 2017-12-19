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
 * interrupt.c - interrupt handling routines
 */

#include <kernel.h>
#include <irq.h>
#include <locore.h>
#include <cpu.h>

/* Registers for interrupt control unit - enable/flag/master */
#define ICU_IRQSTS	(*(volatile uint32_t *)(ICU_BASE + 0x00))
#define ICU_IRQEN	(*(volatile uint32_t *)(ICU_BASE + 0x08))
#define ICU_IRQENSET	(*(volatile uint32_t *)(ICU_BASE + 0x08))
#define ICU_IRQENCLR	(*(volatile uint32_t *)(ICU_BASE + 0x0C))

/*
 * Interrupt Priority Level
 *
 * Each interrupt has its logical priority level, with 0 being
 * the lowest priority. While some ISR is running, all lower
 * priority interrupts are masked off.
 */
volatile int irq_level;

/*
 * Interrupt mapping table
 */
static int ipl_table[NIRQS];		/* vector -> level */
static uint32_t mask_table[NIPLS];	/* level -> mask */

/*
 * Set mask for current ipl
 */
static void
update_mask(void)
{
	u_int mask = mask_table[irq_level];

	ICU_IRQENCLR = ~mask;
	ICU_IRQENSET = mask;
}

/*
 * Unmask interrupt in ICU for specified irq.
 * The interrupt mask table is also updated.
 * Assumes CPU interrupt is disabled in caller.
 */
void
interrupt_unmask(int vector, int level)
{
	int i;
	uint32_t unmask = (uint32_t)1 << vector;

	/* Save level mapping */
	ipl_table[vector] = level;

	/*
	 * Unmask the target interrupt for all
	 * lower interrupt levels.
	 */
	for (i = 0; i < level; i++)
		mask_table[i] |= unmask;
	update_mask();
}

/*
 * Mask interrupt in ICU for specified irq.
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
 * Common interrupt handler.
 */
void
interrupt_handler(void)
{
	uint32_t bits;
	int vector, old_ipl, new_ipl;

	/* Get interrupt source */
	bits = ICU_IRQSTS;
	for (vector = 0; vector < NIRQS; vector++) {
		if (bits & (uint32_t)(1 << vector))
			break;
	}
	if (vector == NIRQS)
		goto out;

	/* Adjust interrupt level */
	old_ipl = irq_level;
	new_ipl = ipl_table[vector];
	if (new_ipl > old_ipl)		/* Ignore spurious interrupt */
		irq_level = new_ipl;
	update_mask();

	/* Dispatch interrupt */
	interrupt_enable();
	irq_handler(vector);
	interrupt_disable();

	/* Restore interrupt level */
	irq_level = old_ipl;
	update_mask();
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

	ICU_IRQENCLR = 0xffff;		/* Mask all interrupts */
}
