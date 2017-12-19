/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * Exec server - Execute various types of image files.
 */

#include <prex/prex.h>
#include <prex/capability.h>
#include <server/fs.h>
#include <server/proc.h>
#include <server/stdmsg.h>
#include <server/object.h>
#include <sys/list.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "exec.h"

/*
 * Object for system server
 */
object_t proc_obj;	/* Process server */
object_t fs_obj;	/* File system server */

/*
 * File header
 */
static char header[HEADER_SIZE];

/*
 * Wait until specified server starts.
 */
static void
wait_server(const char *name, object_t *obj)
{
	int i, err = 0;

	/*
	 * Check the server existence. Timeout is 1sec.
	 */
	for (i = 0; i < 100; i++) {
		err = object_lookup((char *)name, obj);
		if (err == 0)
			break;

		/* Wait 10msec */
		timer_sleep(10, 0);
		thread_yield();
	}
	if (err)
		sys_panic("exec: server not found");
}

static void
process_init(void)
{
	struct msg m;

	m.hdr.code = PS_REGISTER;
	msg_send(proc_obj, &m, sizeof(m));
}

/*
 * Notify exec() to servers.
 */
static void
notify_server(task_t org_task, task_t new_task, void *stack)
{
	struct msg m;
	int err;

	/* Notify to file system server */
	do {
		m.hdr.code = FS_EXEC;
		m.data[0] = (int)org_task;
		m.data[1] = (int)new_task;
		err = msg_send(fs_obj, &m, sizeof(m));
	} while (err == EINTR);

	/* Notify to process server */
	do {
		m.hdr.code = PS_EXEC;
		m.data[0] = (int)org_task;
		m.data[1] = (int)new_task;
		m.data[2] = (int)stack;
		err = msg_send(proc_obj, &m, sizeof(m));
	} while (err == EINTR);
}

/*
 * Execute program
 */
static int
do_exec(struct exec_msg *msg)
{
	struct exec_loader *ldr;
	char *name;
	int err, fd, count;
	struct stat st;
	task_t old_task, new_task;
	thread_t th;
	void *stack, *sp;
	void (*entry)(void);
	cap_t cap;

	DPRINTF(("do_exec: path=%s task=%x\n", msg->path, msg->hdr.task));

	old_task = msg->hdr.task;

	/*
	 * Check capability of caller task.
	 */
	if (task_getcap(old_task, &cap)) {
		err = EINVAL;
		goto err1;
	}
	if ((cap & CAP_EXEC) == 0) {
		err = EPERM;
		goto err1;
	}
	/*
	 * Check target file
	 */
	if ((fd = open(msg->path, O_RDONLY)) == -1) {
		err = ENOENT;
		goto err1;
	}
	if (fstat(fd, &st) == -1) {
		err = EIO;
		goto err2;
	}
	if (!S_ISREG(st.st_mode)) {
		err = EACCES;	/* must be regular file */
		goto err2;
	}
	/*
	 * Find file loader from the file header.
	 */
	if ((count = read(fd, header, HEADER_SIZE)) == -1) {
		err = EIO;
		goto err2;
	}
	for (ldr = loader_table; ldr->el_name != NULL; ldr++) {
		if (ldr->el_probe(header) == 0)
			break;
		/* Check next format */
	}
	if (ldr->el_name == NULL) {
		DPRINTF(("Unsupported file format\n"));
		err = ENOEXEC;
		goto err2;
	}
	DPRINTF(("exec loader=%s\n", ldr->el_name));

	/*
	 * Suspend old task
	 */
	if ((err = task_suspend(old_task)) != 0)
		goto err2;

	/*
	 * Create new task
	 */
	if ((err = task_create(old_task, VM_NEW, &new_task)) != 0)
		goto err2;

	if (msg->path[0] != '\0') {
		name = strrchr(msg->path, '/');
		if (name)
			name++;
		else
			name = msg->path;
		task_name(new_task, name);
	}
	/*
	 * Copy capabilities
	 */
	task_getcap(old_task, &cap);

	/*
	 * XXX: Temporary removed...
	 */
	/* cap &= CONFIG_CAP_MASK; */

	task_setcap(new_task, &cap);

	if ((err = thread_create(new_task, &th)) != 0)
		goto err3;

	/*
	 * Allocate stack and build arguments on it.
	 */
	err = vm_allocate(new_task, &stack, USTACK_SIZE, 1);
	if (err)
		goto err4;
	if ((err = build_args(new_task, stack, msg, &sp)) != 0)
		goto err5;

	/*
	 * Load file image.
	 */
	if ((err = ldr->el_load(header, new_task, fd, (void **)&entry)) != 0)
		goto err5;
	if ((err = thread_load(th, entry, sp)) != 0)
		goto err5;

	/*
	 * Notify to servers.
	 */
	notify_server(old_task, new_task, stack);

	/*
	 * Terminate old task.
	 */
	task_terminate(old_task);

	/*
	 * Set him running.
	 */
	thread_resume(th);

	close(fd);
	DPRINTF(("exec complete successfully\n"));
	return 0;
 err5:
	vm_free(new_task, stack);
 err4:
	thread_terminate(th);
 err3:
	task_terminate(new_task);
 err2:
	close(fd);
 err1:
	DPRINTF(("exec failed err=%d\n", err));
	return err;
}

/*
 * Debug
 */
static void
exec_debug(void)
{

#ifdef DEBUG
	/* mstat(); */
#endif
}

/*
 * Initialize all exec loaders
 */
static void
exec_init(void)
{
	struct exec_loader *ldr;

	for (ldr = loader_table; ldr->el_name; ldr++) {
		DPRINTF(("Initialize \'%s\' loader\n", ldr->el_name));
		ldr->el_init();
	}
}

/*
 * Main routine for exec service.
 */
int
main(int argc, char *argv[])
{
	struct exec_msg msg;
	object_t obj;
	int err;

	sys_log("Starting Exec Server\n");

	/*
	 * Boost current priority
	 */
	thread_setprio(thread_self(), PRIO_EXEC);

	/*
	 * Wait until system server becomes available.
	 */
	wait_server(OBJNAME_PROC, &proc_obj);
	wait_server(OBJNAME_FS, &fs_obj);

	/*
	 * Register to process server
	 */
	process_init();

	/*
	 * Register to file server
	 */
	fslib_init();

	/*
	 * Initialize everything
	 */
	exec_init();

	/*
	 * Create an object to expose our service.
	 */
	err = object_create(OBJNAME_EXEC, &obj);
	if (err)
		sys_panic("fail to create object");

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		err = msg_receive(obj, &msg, sizeof(struct exec_msg));
		if (err)
			continue;
		/*
		 * Process request.
		 */
		err = EINVAL;
		switch (msg.hdr.code) {
		case STD_DEBUG:
			exec_debug();
			err = 0;
			break;

		case EX_EXEC:
			err = do_exec(&msg);
			break;
		}
#ifdef DEBUG_EXEC
		if (err)
			DPRINTF(("msg error=%d\n", err));
#endif
		/*
		 * Reply to the client.
		 */
		msg.hdr.status = err;
		err = msg_reply(obj, &msg, sizeof(struct exec_msg));
	}
	return 0;
}
