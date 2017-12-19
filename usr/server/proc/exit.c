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
 * exit.c - process exit and wait
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "proc.h"

/*
 * Exit process.
 *
 * process_exit() sets the process state to zombie state, and it
 * saves the exit code for waiting process.
 */
int
proc_exit(struct msg *msg)
{
	int exitcode;
	struct proc *child, *parent;
	list_t head, n;

	if (curproc == NULL)
		return EINVAL;

	exitcode = msg->data[0];
	DPRINTF(("exit pid=%d task=%x code=%x\n", curproc->p_pid,
		 msg->hdr.task, exitcode));

	if (curproc->p_stat == SZOMB)
		return EBUSY;

	curproc->p_stat = SZOMB;
	curproc->p_exitcode = exitcode;

	/*
	 * Set the parent pid of all child processes to 1 (init).
	 */
	head = &curproc->p_children;
	n = list_first(head);
	while (n != head) {
		child = list_entry(n, struct proc, p_sibling);
		n = list_next(n);

		child->p_parent = &initproc;
		list_remove(&child->p_sibling);
		list_insert(&initproc.p_children, &child->p_sibling);
	}

	/*
	 * Resume parent process which is wating in vfork.
	 */
	parent = curproc->p_parent;
	if (parent != NULL && parent->p_vforked) {
		vfork_end(parent);

		/*
		 * The child task loses its stack data.
		 * So, it can not run anymore.
		 */
		task_terminate(curproc->p_task);
	}

	/* Send a signal to the parent process. */
	exception_raise(curproc->p_parent->p_task, SIGCHLD);
	return 0;
}

/*
 * Stop process.
 *
 * This is similar with exit(), but it does not update the parent
 * pid of any child processes.
 */
int
proc_stop(struct msg *msg)
{
	int code;

	if (curproc == NULL)
		return EINVAL;

	code = msg->data[0];
	DPRINTF(("stop task=%x code=%x\n", msg->hdr.task, code));

	if (curproc->p_stat == SZOMB)
		return EBUSY;

	curproc->p_stat = SSTOP;
	curproc->p_exitcode = code;

	/* Send a signal to the parent process. */
	exception_raise(curproc->p_parent->p_task, SIGCHLD);
	return 0;
}

/*
 * Find the zombie process in the child processes. It just
 * returns the pid and exit code if it find at least one zombie
 * process.
 *
 * The library stub for waitpid() will wait the SIGCHLD signal in
 * the stub code if there is no zombie process in child process.
 * This signal is sent by proc_exit() or proc_stop() routines in
 * the process server.
 */
int
proc_waitpid(struct msg *msg)
{
	pid_t pid, pid_child;
	int options, code, match;
	struct proc *p;
	list_t head, n;

	if (curproc == NULL)
		return EINVAL;

	pid = (pid_t)msg->data[0];
	options = msg->data[1];
	DPRINTF(("wait task=%x pid=%d options=%x\n", msg->hdr.task,
		 pid, options));

	if (list_empty(&curproc->p_children))
		return ECHILD;	/* No child process */

	/* Set the default pid and exit code */
	pid_child = 0;
	code = 0;

	/*
	 * Check all processes.
	 */
	p = NULL;
	head = &curproc->p_children;
	for (n = list_first(head); n != head; n = list_next(n)) {
		p = list_entry(n, struct proc, p_sibling);

		/*
		 * Check if pid matches.
		 */
		match = 0;
		if (pid > 0) {
			/*
			 * Wait a specific child process.
			 */
			if (p->p_pid == pid)
				match = 1;
		} else if (pid == 0) {
			/*
			 * Wait a process who has same pgid.
			 */
			if (p->p_pgrp->pg_pgid == curproc->p_pgrp->pg_pgid)
				match = 1;
		} else if (pid != -1) {
			/*
			 * Wait a specific pgid.
			 */
			if (p->p_pgrp->pg_pgid == -pid)
				match = 1;
		} else {
			/*
			 * pid = -1 means wait any child process.
			 */
			match = 1;
		}
		if (match) {
			/*
			 * Get the exit code.
			 */
			if (p->p_stat == SSTOP) {
				pid_child = p->p_pid;
				code = p->p_exitcode;
				break;
			} else if (p->p_stat == SZOMB) {
				pid_child = p->p_pid;
				code = p->p_exitcode;
				proc_cleanup(p);
				break;
			}
		}
	}
	msg->data[0] = pid_child;
	msg->data[1] = code;
	return 0;
}
