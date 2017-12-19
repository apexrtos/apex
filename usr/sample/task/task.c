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
 * task.c - a sample program to run tasks.
 */

#include <prex/prex.h>
#include <stdio.h>

#define NR_TASKS 10

/*
 * Create new task
 */
static task_t
task_run(void (*func)(void), char *stack)
{
	task_t task;
	thread_t th;

	if (task_create(task_self(), VM_COPY, &task) != 0)
		return 0;

	if (thread_create(task, &th) != 0)
		return 0;

	if (thread_load(th, func, stack) != 0)
		return 0;

	if (thread_resume(th) != 0)
		return 0;

	return task;
}

/*
 * Function to be called in new task.
 */
static void
hey_yo(void)
{
	task_t self;

	/*
	 * Display string
	 */
	self = task_self();
	printf("Task %x: Hey, Yo!\n", (u_int)self);

	/*
	 * Wait 5 sec
	 */
	timer_sleep(5000, 0);

	/*
	 * Terminate current task.
	 */
	printf("Task %x: Bye!\n", (u_int)self);
	task_terminate(task_self());
}

int
main(int argc, char *argv[])
{
	static char stack[1024];
	int i;

	printf("Task sample program\n");

	/*  Create new tasks */
	for (i = 0; i < NR_TASKS; i++) {
		if (task_run(hey_yo, stack + 1024) == 0)
			panic("failed to create task");
	}

	/* Wait here... */
	task_suspend(task_self());
	return 0;
}
