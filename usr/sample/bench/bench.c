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
 * bench.c - benchmark program for running many threads
 */

/*
 * Note: The system must have enough memory to run this program.
 * At least, 512M bytes of RAM is required to create 100000 threads
 * with i386-pc.
 */

#include <prex/prex.h>
#include <stdio.h>

/*
 * Number of threads
 */
#define NR_THREADS 10000

static thread_t *th;

void
null_thread(void)
{
	for (;;) ;
}

int
main(int argc, char *argv[])
{
	struct info_timer info;
	task_t task;
	char stack[16];
	u_long start, end;
	int i, prio, err;

	printf("Benchmark to create/terminate %d threads\n", NR_THREADS);

	sys_info(INFO_TIMER, &info);
	if (info.hz == 0)
		panic("can not get timer tick rate");

	thread_getprio(thread_self(), &prio);
	thread_setprio(thread_self(), prio - 1);

	task = task_self();
	err = vm_allocate(task, (void **)&th, sizeof(thread_t) * NR_THREADS, 1);
	if (err)
		panic("vm_allocate is failed");

	sys_time(&start);

	/*
	 * Create threads
	 */
	for (i = 0; i < NR_THREADS; i++) {
		if (thread_create(task, &th[i]) != 0)
			panic("thread_create is failed");

		if (thread_load(th[i], null_thread, &stack) != 0)
			panic("thread_load is failed");

		if (thread_resume(th[i]) != 0)
			panic("thread_resume is failed");
	}

	/*
	 * Teminate threads
	 */
	for (i = 0; i < NR_THREADS; i++)
		thread_terminate(th[i]);

	sys_time(&end);

	vm_free(task, th);

	printf("Complete. The score is %d msec (%d ticks).\n",
	       (int)((end - start) * 1000 / info.hz),
	       (int)(end - start));

	return 0;
}
