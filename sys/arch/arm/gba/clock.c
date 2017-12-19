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
 * clock.c - clock driver
 */

#include <kernel.h>
#include <timer.h>
#include <irq.h>

/* Interrupt vector for timer (TMR0 of GBA) */
#define CLOCK_IRQ	3

/* The clock rate per second ... 2^24 */
#define CLOCK_RATE	16777216L

/* The initial counter value */
#define TIMER_COUNT	(0xffff - (CLOCK_RATE / 64 / HZ))

/* GBA timer registers */
#define TMR0_COUNT	(*(volatile uint16_t *)0x4000100)
#define TMR0_CTRL	(*(volatile uint16_t *)0x4000102)

/* Timer frequency */
#define TMR_1_CLOCK	0x0000
#define TMR_64_CLOCK	0x0001
#define TMR_256_CLOCK	0x0002
#define TMR_1024_CLOCK	0x0003

/* Cascade switch */
#define TMR_CASCADE	0x0004

/* Interrupt for overflow */
#define	TMR_IRQEN	0x0040

/* Timer switch */
#define TMR_EN		0x0080

/*
 * Clock interrupt service routine.
 * No H/W reprogram is required.
 */
static int
clock_isr(int irq)
{

	irq_lock();
	timer_tick();
	irq_unlock();
	return INT_DONE;
}

/*
 * Initialize clock H/W chip.
 * Setup clock tick rate and install clock ISR.
 */
void
clock_init(void)
{
	irq_t clock_irq;

	/* Setup counter value */
	TMR0_COUNT = TIMER_COUNT;
	TMR0_CTRL = (uint16_t)(TMR_IRQEN | TMR_64_CLOCK);

	/* Install ISR */
	clock_irq = irq_attach(CLOCK_IRQ, IPL_CLOCK, 0, &clock_isr, NULL);
	ASSERT(clock_irq != NULL);

	/* Enable timer */
	TMR0_CTRL |= TMR_EN;
}
