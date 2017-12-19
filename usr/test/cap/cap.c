/*-
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
 * cap.c - test program for task capability.
 */

#include <prex/prex.h>
#include <prex/capability.h>

#include <stdio.h>

static task_t task;
static cap_t cap;

static void
cap_get(void)
{
	int err;

	if ((err = task_getcap(task, &cap)) != 0) {
		printf("err=%d\n", err);
		panic("cap: failed to get capability\n");
	}
	printf(" capability=%x\n", cap);
}

static void
cap_remove(int c)
{
	int err;

	cap &= ~c;
	if ((err = task_setcap(task, &cap)) != 0) {
		printf("err=%d\n", err);
		panic("cap: failed to change capability\n");
	}
}

int
main(int argc, char *argv[])
{
	task_t child;
	thread_t th;
	int err;

	printf("cap - test program for capability\n");

	task = task_self();
	printf("task=%x\n", (u_int)task);

	if (task_create(task, VM_NEW, &child) != 0)
		panic("failed to create task");

	if (thread_create(child, &th) != 0)
		panic("failed to create thread");

	printf("Get capability\n");
	cap_get();

	/*
	 * Test CAP_TASK
	 */
	printf("Set task name\n");
	if ((err = task_name(child, "foo")) != 0)
		panic("failed to set task name\n");

	printf("\nRemove CAP_TASK\n");
	cap_remove(CAP_TASK);
	cap_get();
	printf(" - OK!\n");

	printf("Set task name\n");
	err = task_name(child, "foo");
	if (err)
		printf("task_name() returns error=%d\n", err);
	else
		panic("task_name() must return error");


	/*
	 * Test CAP_NICE
	 */
	printf("Set priority\n");
	err = thread_setprio(th, 199);
	if (err)
		panic("failed to set priority\n");

	printf("\nRemove CAP_NICE\n");
	cap_remove(CAP_NICE);
	cap_get();
	printf(" - OK!\n");

	printf("Set priority\n");
	err = thread_setprio(th, 199);
	if (err)
		printf("thread_setprio() returns error=%d\n", err);
	else
		panic("thread_setprio() must return error");


	/*
	 * Test CAP_SETPCAP
	 */
	printf("\nRemove CAP_SETPCAP\n");
	cap_remove(CAP_SETPCAP);
	cap_get();
	printf(" - OK!\n");

	printf("\nTest CAP_SETPCAP\n");

	/* This cause panic if it finishes successfully. */
	cap_remove(CAP_SETPCAP);
	cap_get();

	printf(" - Oops!\n");
	return 0;
}
