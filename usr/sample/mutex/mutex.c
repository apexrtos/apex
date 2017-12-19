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
 * mutex.c - sample program for mutex with priority inheritance.
 */

/*
 * Senario:
 * This sample shows how the mutex priority is changed when three
 * different threads lock two mutexes at same time.
 *
 * The priority of each thread are as follows.
 *    Thread1 - priority 100 (highest)
 *    Thread2 - priority 101
 *    Thread3 - priority 102
 *
 * Thread priority and state are changed as follows.
 *
 *    Action                    Thread 1  Thread 2  Thread 3  Mutex A  Mutex B
 *    ------------------------  --------  --------  --------  -------  -------
 * 1) Thread 3 locks mutex A    susp/100  susp/101  run /102  owner=3
 * 2) Thread 2 locks mutex B    susp/100  run /101  run /102  owner=3  owner=2
 * 3) Thread 2 locks mutex A    susp/100  wait/101  run /101* owner=3  owner=2
 * 4) Thread 1 locks mutex B    wait/100  wait/100* run /100* owner=3  owner=2
 * 5) Thread 3 unlocks mutex A  wait/100  run /100  run /102* owner=2* owner=2
 * 6) Thread 2 unlocks mutex B  run /100* run /100  run /102  owner=2  owner=1*
 * 7) Thread 2 unlocks mutex A  run /100  run /100  run /102           owner=1
 * 8) Thread 1 unlocks mutex B  wait/100  run /101  run /102
 *
 */

#include <prex/prex.h>
#include <stdio.h>

static char stack[3][1024];
static thread_t th_1, th_2, th_3;
static mutex_t mtx_A, mtx_B;

static void
dump_prio(void)
{
	int prio;

	if (thread_getprio(th_1, &prio) == 0)
		printf("th_1: prio=%d\n", prio);

	if (thread_getprio(th_2, &prio) == 0)
		printf("th_2: prio=%d\n", prio);
	
	if (thread_getprio(th_3, &prio) == 0)
		printf("th_3: prio=%d\n", prio);
}

thread_t
thread_run(void (*start)(void), void *stack)
{
	thread_t th;

	if (thread_create(task_self(), &th) != 0)
		panic("thread_create is failed");

	if (thread_load(th, start, stack) != 0)
		panic("thread_load is failed");

	return th;
}

/*
 * Thread 1 - Priority = 100
 */
static void
thread_1(void)
{
	printf("thread_1: starting\n");

	/*
	 * 4) Lock mutex B
	 *
	 * Priority inheritance:
	 *    Thread 2... Prio 101 -> 100
	 *    Thread 3... Prio 101 -> 100
	 */
	printf("thread_1: 4) lock B\n");
	mutex_lock(&mtx_B);

	printf("thread_1: running\n");
	dump_prio();

	/*
	 * 8) Unlock mutex B
	 */
	printf("thread_1: 7) unlock B\n");
	mutex_unlock(&mtx_B);

	dump_prio();
	printf("thread_1: exit\n");
	thread_terminate(th_1);
}

/*
 * Thread 2 - Priority = 101
 */
static void
thread_2(void)
{
	printf("thread_2: starting\n");

	/*
	 * 2) Lock mutex B
	 */
	printf("thread_2: 2) lock B\n");
	mutex_lock(&mtx_B);
	dump_prio();

	/*
	 * 3) Lock mutex A (Switch to thread 3)
	 *
	 * Priority inheritance:
	 *    Thread 3... Prio 102 -> 101
	 */
	printf("thread_2: 3) lock A\n");
	mutex_lock(&mtx_A);

	printf("thread_2: running\n");
	dump_prio();

	/*
	 * 6) Unlock mutex B
	 */
	printf("thread_2: 6) unlock B\n");
	mutex_unlock(&mtx_B);

	dump_prio();

	/*
	 * 7) Unlock mutex A
	 */
	printf("thread_2: 8) unlock A\n");
	mutex_unlock(&mtx_A);

	printf("thread_2: exit\n");
	thread_terminate(th_2);
}


/*
 * Thread 3 - Priority = 102
 */
static void
thread_3(void)
{
	printf("thread_3: start\n");

	/*
	 * 1) Lock mutex A
	 */
	printf("thread_3: 1) lock A\n");
	mutex_lock(&mtx_A);
	dump_prio();

	/*
	 * Start thread 2
	 */
	thread_resume(th_2);

	/*
	 * Check priority
	 */
	printf("thread_3: running-1\n");
	dump_prio();

	/*
	 * Start thread 1
	 */
	thread_resume(th_1);
	printf("thread_3: running-2\n");
	dump_prio();

	/*
	 * 5) Unlock mutex A
	 */
	printf("thread_3: 5) unlock A\n");
	mutex_unlock(&mtx_A);

	dump_prio();
	printf("thread_3: exit\n");
	thread_terminate(th_3);
}

int
main(int argc, char *argv[])
{
	printf("Mutex sample program\n");

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

	th_3 = thread_run(thread_3, stack[2]+1024);
	thread_setprio(th_3, 102);

	dump_prio();

	/*
	 * Start lowest priority thread
	 */
	thread_resume(th_3);

	/*
	 * Wait...
	 */
	thread_suspend(thread_self());

	return 0;
}
