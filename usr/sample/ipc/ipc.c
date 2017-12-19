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
 * ipc.c - A sample program for IPC message transmission.
 */

#include <prex/prex.h>
#include <prex/message.h>
#include <stdio.h>
#include <string.h>

/*
 * Message structure
 */
struct chat_msg {
	struct msg_header hdr;	/* Message header */
	char	str[128];	/* String */
};


static char stack[1024];

/*
 * Start client task/thread
 */
static task_t
start_client(void (*func)(void), char *stack)
{
	task_t task;
	thread_t th;

#ifdef CONFIG_MMU
	if (task_create(task_self(), VM_COPY, &task) != 0)
		return 0;
#else
	task = task_self();
#endif
	if (thread_create(task, &th) != 0)
		return 0;

	if (thread_load(th, func, stack) != 0)
		return 0;

	if (thread_resume(th) != 0)
		return 0;

	return task;
}

/*
 * Send message to server
 */
void
send_message(object_t obj, const char *str)
{
	struct chat_msg msg;

	timer_sleep(2000, 0);
	strcpy(msg.str, str);
	msg_send(obj, &msg, sizeof(msg));
	printf("client: Received \"%s\"\n", msg.str);
}


/*
 * Client task
 */
static void
client_task(void)
{
	object_t obj;
	int err;

	printf("Client is started\n");

	/*
	 * Find objects.
	 */
	if ((err = object_lookup("/foo/bar", &obj)) != 0)
		panic("can not find object");

	/*
	 * Send message per 2 second.
	 */
	send_message(obj, "Hello!");
	send_message(obj, "This is a client task.");
	send_message(obj, "Who are you?");
	send_message(obj, "How are you?");
	send_message(obj, "....");
	send_message(obj, "Bye!");
	send_message(obj, "Exit");

#ifdef CONFIG_MMU
	printf("Exit client task...\n");
	task_terminate(task_self());
#endif
}

int
main(int argc, char *argv[])
{
	object_t obj;
	struct chat_msg msg;
	int err, exit;

	printf("IPC sample program\n");

	/*
	 * Boost priority of this thread
	 */
	thread_setprio(thread_self(), 90);

	/*
	 * Create object
	 */
	if ((err = object_create("/foo/bar", &obj)) != 0)
		panic("fail to create object");

	/*
	 * Start client task.
	 */
	if (start_client(client_task, stack+1024) == 0)
		panic("fail to create task");

	/*
	 * Message loop
	 */
	exit = 0;
	while (exit == 0) {
		/*
		 * Wait for an incoming request.
		 */
		err = msg_receive(obj, &msg, sizeof(msg));
		if (err)
			continue;
		/*
		 * Process request.
		 */
		printf("server: Received \"%s\"\n", msg.str);
		if (!strcmp(msg.str, "Exit"))
			exit = 1;

		if (!strcmp(msg.str, "Hello!"))
			strcpy(msg.str, "Hi.");
		else if (!strcmp(msg.str, "Bye!"))
			strcpy(msg.str, "Bye.");
		else
			strcpy(msg.str, "OK.");

		/*
		 * Reply to the client.
		 */
		msg_reply(obj, &msg, sizeof(msg));
	}
	timer_sleep(1000, 0);
	printf("End...\n");
	return 0;
}
