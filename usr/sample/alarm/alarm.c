/*
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
 * alarm.c - sample alarm program.
 */

/*
 * Expected output:
 *
 * Alarm sample program
 * Start alarm timer
 * Ring! count=1 time=1000 msec
 * Ring! count=2 time=1200 msec
 * Ring! count=3 time=1600 msec
 * Ring! count=4 time=2200 msec
 * Ring! count=5 time=3000 msec
 * Ring! count=6 time=4000 msec
 * Ring! count=7 time=5200 msec
 * Ring! count=8 time=6600 msec
 * Ring! count=9 time=8200 msec
 * Ring! count=10 time=10000 msec
 * End...
 *
 */

#include <prex/prex.h>
#include <sys/signal.h>
#include <stdio.h>

static int hz;
static int count;
static u_long start_tick;

/*
 * Re-program alarm timer as count * 200msec.
 */
static void
alarm_handler(int code)
{
	u_long tick;

	if (code == SIGALRM) {
		count++;
		if (count > 10) {
			printf("End...\n");
			task_terminate(task_self());
		}
		timer_alarm(count * 200, 0);
		sys_time(&tick);
		printf("Ring! count=%d time=%d msec\n", count,
		       (u_int)((tick - start_tick) * 1000 / hz));
	}
	exception_return();
}

int
main(int argc, char *argv[])
{
	struct info_timer info;

	printf("Alarm sample program\n");

	sys_info(INFO_TIMER, &info);
	if (info.hz == 0)
		panic("can not get timer tick rate");
	hz = info.hz;

	/*
	 * Install alarm handler
	 */
	exception_setup(alarm_handler);
	exception_raise(task_self(), 0xfff);
	timer_sleep(2000, 0);

	/*
	 * Kick first alarm timer.
	 */
	printf("Start alarm timer\n");
	count = 0;
	sys_time(&start_tick);
	timer_alarm(1000, 0);

	/* Wait alarm */
	for (;;);

	return 0;
}
