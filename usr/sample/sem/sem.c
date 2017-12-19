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
 * sem.c - sample program for semaphore.
 */

#include <prex/prex.h>
#include <stdio.h>

static sem_t sem;

void
thread_run(void (*start)(void), void *stack)
{
	thread_t th;

	if (thread_create(task_self(), &th) != 0)
		panic("thread_create is failed");

	if (thread_load(th, start, stack) != 0)
		panic("thread_load is failed");

	if (thread_resume(th) != 0)
		panic("thread_resume failed");

	return;
}

/*
 * The main routine create 10 copy of this thread. But, since the
 * initial semaphore value is 3, only 3 threads can run at same time.
 */
static void
new_thread(void)
{
	thread_t th;

	th = thread_self();
	printf("Start thread=%x\n", (u_int)th);
	thread_yield();

	/*
	 * Acquire semaphore
	 */
	sem_wait(&sem, 0);

	/*
	 * Sleep 2000 ms
	 */
	printf("Running thread=%x\n", (u_int)th);
	timer_sleep(2000, 0);

	/*
	 * Release semaphore
	 */
	sem_post(&sem);	

	printf("End thread=%x\n", (u_int)th);
	thread_terminate(th);
}

int
main(int argc, char *argv[])
{
	static char stack[10][1024];
	int i;

	printf("Semaphore sample program\n");

	/*
	 * Initialize semaphore with initial count 3
	 */
	sem_init(&sem, 3);

	/*
	 * Boost the priority of this thread
	 */
	thread_setprio(thread_self(), 100);

	/*
	 * Create 10 threads
	 */
	for (i = 0; i < 10; i++)
		thread_run(new_thread, stack[i]+1024);

	/*
	 * Wait...
	 */
	thread_suspend(thread_self());

	return 0;
}
