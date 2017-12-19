/*
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * thread.c - test to run threads
 */

#include <prex/prex.h>
#include <stdio.h>

static char stack[1024];

static thread_t
thread_run(void (*start)(void), char *stack)
{
	thread_t th;
	int err;

	if ((err = thread_create(task_self(), &th)) != 0)
		panic("thread_create() is failed");

	if ((err = thread_load(th, start, stack)) != 0)
		panic("thread_load() is failed");

	if ((err = thread_resume(th)) != 0)
		panic("thread_resume() is failed");

	return th;
}

static void
test_thread(void)
{
	printf("test thread is starting...\n");
	for (;;)
		putchar('@');
}

int
main(int argc, char *argv[])
{
	int err;
	thread_t self, th;

	printf("Thread test program\n");

	self = thread_self();

	/*
	 * Create new thread
	 */
	printf("Start test thread\n");
	th = thread_run(test_thread, stack+1024);

	/*
	 * Wait 1 sec
	 */
	timer_sleep(1000, 0);

	/*
	 * Suspend test thread
	 */
	printf("\nSuspend test thread\n");
	err = thread_suspend(th);

	/*
	 * Wait 2 sec
	 */
	timer_sleep(2000, 0);

	/*
	 * Resume test thread
	 */
	printf("\nResume test thread\n");
	err = thread_resume(th);

	/*
	 * Wait 100 msec
	 */
	timer_sleep(100, 0);

	/*
	 * Suspend test thread
	 */
	thread_suspend(th);

	/*
	 * Wait 2 sec
	 */
	timer_sleep(2000, 0);

	/*
	 * Resume test thread
	 */
	thread_resume(th);

	/*
	 * We can check this thread can run 10 times than test thread,
	 */
	for (;;)
		putchar('!');

	return 0;
}
