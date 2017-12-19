/*
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * Process server:
 *
 * A process server is responsible to handle process ID, group
 * ID, signal and fork()/exec() state. Since Prex microkernel
 * does not have the concept about process or process group, the
 * process server will map each Prex task to POSIX process.
 *
 * Prex does not support uid (user ID) and gid (group ID) because
 * it runs only in a single user mode. The value of uid and gid is
 * always returned as 1 for all process. These are handled by the
 * library stubs, and it is out of scope in this server.
 *
 * Important Notice:
 * This server is made as a single thread program to reduce many
 * locks and to keep the code clean. So, we should not block in
 * the kernel for any service. If some service must wait an
 * event, it should wait within the library stub in the client
 * application.
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <server/stdmsg.h>
#include <server/object.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "proc.h"

/* forward declarations */
static int proc_debug(struct msg *);
static int proc_shutdown(struct msg *);
static int proc_exec(struct msg *);
static int proc_pstat(struct msg *);
static int proc_register(struct msg *);
static int proc_setinit(struct msg *);

/*
 * Message mapping
 */
struct msg_map {
	int code;
	int (*func)(struct msg *);
};

static const struct msg_map procmsg_map[] = {
	{STD_DEBUG,	proc_debug},
	{STD_SHUTDOWN,	proc_shutdown},
	{PS_GETPID,	proc_getpid},
	{PS_GETPPID,	proc_getppid},
	{PS_GETPGID,	proc_getpgid},
	{PS_SETPGID,	proc_setpgid},
	{PS_GETSID,	proc_getsid},
	{PS_SETSID,	proc_setsid},
	{PS_FORK,	proc_fork},
	{PS_EXIT,	proc_exit},
	{PS_STOP,	proc_stop},
	{PS_WAITPID,	proc_waitpid},
	{PS_KILL,	proc_kill},
	{PS_EXEC,	proc_exec},
	{PS_PSTAT,	proc_pstat},
	{PS_REGISTER,	proc_register},
	{PS_SETINIT,	proc_setinit},
	{0,		NULL},
};

static struct proc proc0;	/* process data of this server (pid=0) */
static struct pgrp pgrp0;	/* process group for first process */
static struct session session0;	/* session for first process */

struct proc initproc;		/* process slot for init process (pid=1) */
struct proc *curproc;		/* current (caller) process */
struct list allproc;		/* list of all processes */

/*
 * Create a new process.
 */
static void
newproc(struct proc *p, pid_t pid, task_t task)
{

	p->p_parent = &proc0;
	p->p_pgrp = &pgrp0;
	p->p_stat = SRUN;
	p->p_exitcode = 0;
	p->p_vforked = 0;
	p->p_pid = pid;
	p->p_task = task;
	list_init(&p->p_children);
	list_insert(&allproc, &p->p_link);
	proc_add(p);
	list_insert(&proc0.p_children, &p->p_sibling);
	list_insert(&pgrp0.pg_members, &p->p_pgrp_link);
}

/*
 * exec() - Update pid to track the mapping with task id.
 * The almost all work is done by a exec server for exec()
 * emulation. So, there is not so many jobs here...
 */
static int
proc_exec(struct msg *msg)
{
	task_t orgtask, newtask;
	struct proc *p, *parent;

	DPRINTF(("proc_exec: pid=%x\n", curproc->p_pid));

	orgtask = (task_t)msg->data[0];
	newtask = (task_t)msg->data[1];
	if ((p = task_to_proc(orgtask)) == NULL)
		return EINVAL;

	proc_remove(p);
	p->p_task = newtask;
	proc_add(p);
	p->p_stackbase = (void *)msg->data[2];

	parent = p->p_parent;
	if (parent != NULL && parent->p_vforked)
		vfork_end(parent);
	return 0;
}

/*
 * Get process status.
 */
static int
proc_pstat(struct msg *msg)
{
	task_t task;
	struct proc *p;

	DPRINTF(("proc_pstat: task=%x\n", msg->data[0]));

	task = (task_t)msg->data[0];
	if ((p = task_to_proc(task)) == NULL)
		return EINVAL;

	msg->data[0] = (int)p->p_pid;
	msg->data[2] = (int)p->p_stat;
	if (p->p_parent == NULL)
		msg->data[1] = (int)0;
	else
		msg->data[1] = (int)p->p_parent->p_pid;
	return 0;
}

/*
 * Set init process (pid=1).
 */
static int
proc_setinit(struct msg *msg)
{
	struct proc *p;

	DPRINTF(("proc_setinit\n"));

	p = &initproc;
	if (p->p_stat == SRUN)
		return EPERM;

	newproc(p, 1, msg->hdr.task);
	return 0;
}

/*
 * Register boot task.
 */
static int
proc_register(struct msg *msg)
{
	struct proc *p;
	pid_t pid;

	DPRINTF(("proc_register\n"));
	if ((p = malloc(sizeof(struct proc))) == NULL)
		return ENOMEM;
	memset(p, 0, sizeof(struct proc));

	if ((pid = pid_assign()) == 0) {
		free(p);
		return EAGAIN;	/* Too many processes */
	}
	newproc(p, pid, msg->hdr.task);
	DPRINTF(("proc_register-comp\n"));
	return 0;
}

static int
proc_shutdown(struct msg *msg)
{

	return 0;
}

static int
proc_debug(struct msg *msg)
{
#ifdef DEBUG_PROC
	struct proc *p;
	list_t n;
	char stat[][5] = { "    ", "RUN ", "ZOMB", "STOP" };

	dprintf("<Process Server>\n");
	dprintf("Dump process\n");
	dprintf(" pid    ppid   pgid   sid    stat task\n");
	dprintf(" ------ ------ ------ ------ ---- --------\n");

	for (n = list_first(&allproc); n != &allproc;
	     n = list_next(n)) {
		p = list_entry(n, struct proc, p_link);
		dprintf(" %6d %6d %6d %6d %s %08x\n", p->p_pid,
			p->p_parent->p_pid, p->p_pgrp->pg_pgid,
			p->p_pgrp->pg_session->s_leader->p_pid,
			stat[p->p_stat], p->p_task);
	}
	dprintf("\n");
#endif
	return 0;
}

static void
init(void)
{
	struct proc *p;

	p = &proc0;
	curproc = p;

	tty_init();
	table_init();
	list_init(&allproc);

	/*
	 * Create process 0 (the process server)
	 */
	pgrp0.pg_pgid = 0;
	list_init(&pgrp0.pg_members);
	pgrp_add(&pgrp0);

	pgrp0.pg_session = &session0;
	session0.s_refcnt = 1;
	session0.s_leader = p;
	session0.s_ttyhold = 0;

	p->p_pgrp = &pgrp0;
	p->p_parent = 0;
	p->p_stat = SRUN;
	p->p_exitcode = 0;
	p->p_vforked = 0;
	p->p_pid = 0;
	p->p_task = task_self();
	list_init(&p->p_children);
	proc_add(p);
	list_insert(&pgrp0.pg_members, &p->p_pgrp_link);
}

/*
 * Main routine for process service.
 */
int
main(int argc, char *argv[])
{
	static struct msg msg;
	const struct msg_map *map;
	object_t obj;
	int err;

	sys_log("Starting Process Server\n");

	/*
	 * Boost current priority.
	 */
	thread_setprio(thread_self(), PRIO_PROC);

	/*
	 * Initialize everything.
	 */
	init();

	/*
	 * Create an object to expose our service.
	 */
	if ((err = object_create(OBJNAME_PROC, &obj)) != 0)
		sys_panic("proc: fail to create object");

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		err = msg_receive(obj, &msg, sizeof(msg));
		if (err)
			continue;

		err = EINVAL;
		map = &procmsg_map[0];
		while (map->code != 0) {
			if (map->code == msg.hdr.code) {

				/* Get current process */
				curproc = task_to_proc(msg.hdr.task);

				/* Update the capability of caller task. */
				if (curproc &&
				    task_getcap(msg.hdr.task, &curproc->p_cap))
					break;

				err = (*map->func)(&msg);
				break;
			}
			map++;
		}
		/*
		 * Reply to the client.
		 */
		msg.hdr.status = err;
		msg_reply(obj, &msg, sizeof(msg));
#ifdef DEBUG_PROC
		if (err)
			DPRINTF(("proc: msg code=%x error=%d\n", map->code,
				 err));
#endif
	}
	return 0;
}
