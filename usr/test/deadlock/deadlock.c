/*-
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
 * deadlock.c - test program for a kernel deadlock detection.
 */

/*
 * Prex kernel has the feature of deadlock detection. The following
 * senario is tested with this program.
 *
 * 1) Thread-2 locks mutex-A.
 * 2) Thread-1 locks mutex-B.
 * 3) Thread-1 locks mutex-A.
 * 4) Thread-2 locks mutex-B.
 *
 * The deadlock occurs at 4) because mutex-B has been already locked by
 * thread-1 and thread-1 is waiting for thread-2 (mutex-A).
 * The kernel will detect this condition, and the mutex_lock() system
 * call will return EDEADLK error code.
 */

#include <prex/prex.h>
#include <stdio.h>
#include <errno.h>

static char stack[2][1024];
static thread_t th_1, th_2;
static mutex_t mtx_A, mtx_B;

thread_t
thread_run(void (*start)(void), void *stack)
{
	thread_t th;
	int err;

	err = thread_create(task_self(), &th);
	if (err)
		panic("thread_create is failed");

	err = thread_load(th, start, stack);
	if (err)
		panic("thread_load is failed");

	return th;
}

/*
 * Thread 1 - Priority = 100
 */
static void
thread_1(void)
{
	int err;

	printf("thread_1: starting\n");

	/*
	 * 2) Lock mutex B
	 */
	printf("thread_1: 2) lock B\n");
	err = mutex_lock(&mtx_B);
	if (err) printf("err=%d\n", err);

	/*
	 * 3) Lock mutex A
	 *
	 * Switch to thread 1.
	 */
	printf("thread_1: 3) lock A\n");
	err = mutex_lock(&mtx_A);
	if (err) printf("err=%d\n", err);

	printf("thread_1: exit\n");
	thread_terminate(th_1);
}

/*
 * Thread 2 - Priority = 101
 */
static void
thread_2(void)
{
	int err;

	printf("thread_2: starting\n");

	/*
	 * 1) Lock mutex A
	 */
	printf("thread_2: 1) lock A\n");
	err = mutex_lock(&mtx_A);
	if (err) printf("err=%d\n", err);

	/*
	 * Switch to thread 1
	 */
	thread_resume(th_1);

	printf("thread_2: running\n");
	/*
	 * 4) Lock mutex B
	 *
	 * Deadlock occurs here!
	 */
	printf("thread_2: 4) lock B\n");
	err = mutex_lock(&mtx_B);
	if (err) printf("err=%d\n", err);
	if (err == EDEADLK)
		printf("**** DEADLOCK!! ****\n");

	printf("thread_2: exit\n");
	thread_terminate(th_2);
}

int
main(int argc, char *argv[])
{
	printf("Deadlock test program\n");

	/*
	 * Boost priority of this thread
	 */
	thread_setprio(thread_self(), 90);

	/*
	 * Initialize mutexes.
	 */
	mutex_init(&mtx_A);
	mutex_init(&mtx_B);

	/*
	 * Create new threads
	 */
	th_1 = thread_run(thread_1, stack[0]+1024);
	thread_setprio(th_1, 100);

	th_2 = thread_run(thread_2, stack[1]+1024);
	thread_setprio(th_2, 101);

	/*
	 * Start thread 2
	 */
	thread_resume(th_2);

	/*
	 * Wait...
	 */
	thread_suspend(thread_self());

	return 0;
}
