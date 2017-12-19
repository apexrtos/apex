/*
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * task.c - test program for kernel task services.
 */

#include <prex/prex.h>
#include <stdio.h>
#include <stdlib.h>

#define NR_THREADS 16

static char stack[NR_THREADS][1024];

static void
test_thread(void)
{
	printf("New thread %x is started\n", (u_int)thread_self());

	for (;;) {
		timer_sleep(100, 0);
		printf("@");
	}
}

int
main(int argc, char *argv[])
{
	task_t task;
	thread_t th;
	int i, err;

	printf("Task test program\n");
	sys_log("Task test program\n");

	/*
	 * Boost priority of this thread
	 */
	thread_setprio(thread_self(), 90);

	/*
	 * Create test task
	 */
#ifdef CONFIG_MMU
	err = task_create(task_self(), VM_COPY, &task);
#else
	err = task_create(task_self(), VM_SHARE, &task);
#endif
	if (err) {
		printf("task_create failed. err=%d\n", err);
		exit(1);
	}

	/*
	 * Run threads
	 */
	for (i = 0; i < NR_THREADS; i++) {
		err = thread_create(task, &th);
		printf("thread_create: err=%d\n", err);
		err = thread_load(th, test_thread, stack[i]+1024);
		printf("thread_load: err=%d\n", err);
		thread_resume(th);
	}

	/*
	 * Sleep for a while
	 */
	timer_sleep(1000, 0);

	/*
	 * Suspend test task
	 */
	printf("\nSuspend test task.\n");
	err = task_suspend(task);
	if (err)
		panic("task suspend failed");

	/*
	 * Sleep for a while
	 */
	printf("Sleep\n");
	timer_sleep(500, 0);

	printf("\nResume test task.\n");
	err = task_resume(task);
	if (err)
		panic("task resume failed");

	/*
	 * Sleep for a while
	 */
	timer_sleep(3000, 0);

	printf("\nResume task, again.\n");
	err = task_resume(task);
	if (err)
		printf("Error - OK!\n");

	timer_sleep(1000, 0);

	printf("\nTerminate task.\n");
	task_terminate(task);

	printf("\nTest OK!\n");
	return 0;
}
