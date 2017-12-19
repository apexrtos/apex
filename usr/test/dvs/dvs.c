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
 * dvs.c - test DVS (dynamic voltage scaling).
 */

/*
 * This program changes the CPU load periodically. Since the DVS driver
 * is polling the CPU load, the driver will change the CPU voltage/freqency
 * automatically when CPU becomes idle state.
 *
 * It is recommended to run CPU voltage monitor 'cpumon' with this program.
 */

#include <prex/prex.h>
#include <stdio.h>

static void
set_0_or_100(void)
{
	u_long t1, t2;
	char index[] = "-\\|/";
	char bar[] = "*\033[1D";
	int i, cnt;

	i = 0;
	for (cnt = 0; cnt < 2; cnt++) {
		/*
		 * Display indicator for 5 sec.
		 */
		printf("\rCPU Busy:");
		sys_time(&t1);
		for (;;) {
			sys_time(&t2);
			if (t2 > t1 + 5000)
				break;
			/*
			 * Update indicator per 100 msec.
			 */
			if ((t2 % 100) == 0) {
				bar[0] = index[i++ % 4];
				printf(bar);
			}
		}
		/*
		 * Sleep 5 sec.
		 */
		printf("\rCPU Idle  ");
		timer_sleep(5000, 0);
	}
}

static void
set_50(void)
{
	u_long t1, t2;
	char index[] = "-\\|/";
	char bar[] = "*\033[1D";
	int i, cnt;

	printf("\rCPU half speed:");
	i = 0;
	for (cnt = 0; cnt < 5000; cnt++) {
		/*
		 * Update indicator per 100 msec.
		 */
		if ((t2 % 100) == 0) {
			bar[0] = index[i++ % 4];
			printf(bar);
		}
		sys_time(&t1);
		for (;;) {
			sys_time(&t2);
			if (t2 > t1 + 1)
				break;
		}
		/*
		 * Sleep 1msec.
		 */
		timer_sleep(1, 0);
	}
}

int
main(int argc, char *argv[])
{
	thread_yield();

	printf("DVS test program\n");

	for (;;) {
		set_0_or_100();
		set_50();
	}
	return 0;
}
