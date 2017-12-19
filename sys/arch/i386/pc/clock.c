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
#include <cpufunc.h>

/* Interrupt vector for clock */
#define CLOCK_IRQ	0

/* The internal tick rate in ticks per second */
#define PIT_TICK	1193180L

/* The latch count value for the current HZ setting */
#define PIT_LATCH	((PIT_TICK + (HZ / 2)) / HZ)

/* I/O port for programmable interval timer */
#define PIT_CH0		0x40
#define PIT_CTRL	0x43

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

	outb_p(0x34, PIT_CTRL);		/* Command to set generator mode */
	outb_p((u_char)(PIT_LATCH & 0xff), PIT_CH0);		/* LSB */
	outb_p((u_char)((PIT_LATCH >> 8) & 0xff), PIT_CH0);	/* MSB */

	clock_irq = irq_attach(CLOCK_IRQ, IPL_CLOCK, 0, &clock_isr, NULL);
	ASSERT(clock_irq != NULL);

	DPRINTF(("Clock rate: %d ticks/sec\n", CONFIG_HZ));
}
