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
 * ipc_mt.c - IPC test for multi threaded servers.
 */

#include <prex/prex.h>
#include <server/stdmsg.h>
#include <stdio.h>

#define NR_THREADS	5

static char stack[NR_THREADS][1024];

/*
 * Run specified thread
 */
static int
thread_run(void (*start)(void), void *stack)
{
	thread_t th;
	int err;

	err = thread_create(task_self(), &th);
	if (err)
		return err;

	err = thread_load(th, start, stack);
	if (err)
		return err;

	err = thread_resume(th);
	if (err)
		return err;


	return 0;
}

/*
 * Receiver thread
 */
static void
receive_thread(void)
{
	struct msg msg;
	object_t obj;
	int err;

	printf("Receiver thread is starting...\n");

	thread_setprio(thread_self(), 240);

	/*
	 * Find objects.
	 */
	err = object_lookup("/test/A", &obj);

	for (;;) {
		/*
		 * Receive message from object.
		 */
		printf("Wait message.\n");
		msg_receive(obj, &msg, sizeof(msg));

		printf("Message received.\n");
		/*
		 * Wait a sec.
		 */
		timer_sleep(1000, 0);

		printf("Reply message.\n");
		msg_reply(obj, &msg, sizeof(msg));

		for (;;);
	}
}

int
main(int argc, char *argv[])
{
	object_t obj;
	int err, i;

	printf("IPC test for multi threads\n");

	/*
	 * Create an object.
	 */
	err = object_create("/test/A", &obj);
	if (err)
		panic("failed to create object");

	/*
	 * Start receiver thread.
	 */
	for (i = 0; i < NR_THREADS; i++) {
		err = thread_run(receive_thread, stack[i] + 1024);
		if (err)
			panic("failed to run thread");
	}
	printf("ok?\n");
	thread_setprio(thread_self(), 241);
	for (;;) ;
	return 0;
}
