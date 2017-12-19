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
 * delay.c - driver delay routine
 */

#include <driver.h>

#ifdef DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

static u_long delay_count;	/* Loop count for 1ms */

static
void delay_loop(u_long count)
{
	volatile u_int i;

	for (i = 0; i < count; i++);
}

/*
 * Delay micro seconds (non-blocking)
 * timer_udelay() can be called from ISR at interrupt level.
 */
void
delay_usec(u_long usec)
{

	delay_loop(delay_count * usec / 1000);
}

/*
 * Compute the loop count for 1ms.
 * Assumes clock interrupt is enabled.
 */
void
calibrate_delay(void)
{
	u_long ticks, test_bit;

	DPRINTF(("Calibrating delay loop... "));
	delay_count = 1;
	for (;;) {
		delay_count <<= 1;

		ticks = timer_count();
		while (ticks == timer_count());

		ticks = timer_count();
		delay_loop(delay_count);
		if (ticks != timer_count())
			break;
	}
	delay_count >>= 1;
	test_bit = delay_count;

	for (;;) {
		test_bit >>= 1;
		if (test_bit == 0)
			break;
		delay_count |= test_bit;

		ticks = timer_count();
		while (ticks == timer_count());

		ticks = timer_count();
		delay_loop(delay_count);
		if (ticks != timer_count())
			delay_count &= ~test_bit;
	}
	delay_count *= 1000;	/* count per 1sec */
	delay_count /= 1000;	/* count per 1ms */
	DPRINTF(("ok count=%d\n", delay_count));
}
