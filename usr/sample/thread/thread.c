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
 * thread.c - sample program to create three threads.
 */

#include <prex/prex.h>
#include <stdio.h>

#define VERBOSE_MODE

#ifdef VERBOSE_MODE
#define thr_printf(s) printf(s)
#else
#define thr_printf(s)
#endif

static thread_t main_th;
static char stack[3][1024];

/*
 * Run specified routine as new thread.
 */
static int
thread_run(void (*start)(void), void *stack)
{
	thread_t th;

	if (thread_create(task_self(), &th) != 0)
		return -1;

	if (thread_load(th, start, stack) != 0)
		return -1;

	if (thread_resume(th) != 0)
		return -1;
	return 0;
}

/*
 * Display 'AAAA...'
 */
static void
thread_A(void)
{
	int i;

	thr_printf("\nthread A is starting\n");

	for (i = 0; i < 1024; i++) {
		thr_printf("A");
		if ((i & 0xff) == 0)
			thread_yield();
	}
	thr_printf("\nthread A is terminated\n");
	thread_terminate(thread_self());
}

/*
 * Display 'BBBB...'
 */
static void
thread_B(void)
{
	int i;

	thr_printf("\nthread B is starting\n");

	for (i = 0; i < 4096; i++) {
		thr_printf("B");
		if ((i & 0xff) == 0)
			thread_yield();
	}
	thr_printf("\nthread B is terminated\n");
	thread_terminate(thread_self());
}

/*
 * Display 'CCCC...'
 */
static void
thread_C(void)
{
	int i;

	thr_printf("\nthread C is starting\n");

	for (i = 0; i < 8192; i++) {
		thr_printf("C");
		if ((i & 0xff) == 0)
			thread_yield();
	}
	thr_printf("\nthread C is terminated\n");
	thread_terminate(thread_self());
}

int
main(int argc, char *argv[])
{
	int err;

	printf("Thread sample program\n");
	main_th = thread_self();

	/*
	 * Raise this thread's priority.
	 */
	err = thread_setprio(main_th, 100);

	/*
	 * Run threads as normal priority thread.
	 */
	thread_run(thread_A, stack[0]+1024);
	thread_run(thread_B, stack[1]+1024);
	thread_run(thread_C, stack[2]+1024);

	/*
	 * Lower this thread's priority.
	 */
	err = thread_setprio(main_th, 254);

	/*
	 * Since other threads have higher priority than
	 * this thread, the control will come here only when
	 * all other threads are terminated.
	 */
	printf("test - OK!\n");
	return 0;
}
