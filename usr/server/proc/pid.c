/*
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
 * pid.c - service for process id.
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <server/stdmsg.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "proc.h"

/*
 * PID previously allocated.
 *
 * Note: The following pids are reserved by default.
 *   pid = 0: Process server
 *   pid = 1: Init process
 */
static pid_t last_pid = 1;

/*
 * Assign new pid.
 * Returns pid on sucess, or 0 on failure.
 */
pid_t
pid_assign(void)
{
	pid_t pid;

	pid = last_pid + 1;
	if (pid >= PID_MAX)
		pid = 1;
	while (pid != last_pid) {
		if (proc_find(pid) == NULL)
			break;
		if (++pid >= PID_MAX)
			pid = 1;
	}
	if (pid == last_pid)
		return 0;
	last_pid = pid;
	return pid;
}

/*
 * getpid - get the process ID.
 */
int
proc_getpid(struct msg *msg)
{

	if (curproc == NULL)
		return ESRCH;

	msg->data[0] = (int)curproc->p_pid;
	return 0;
}

/*
 * getppid - get the parent process ID.
 */
int
proc_getppid(struct msg *msg)
{

	if (curproc == NULL)
		return ESRCH;

	msg->data[0] = (int)curproc->p_parent->p_pid;
	return 0;
}

/*
 * getpgid - get the process group ID for a process.
 *
 * If the specified pid is equal to 0, it returns the process
 * group ID of the calling process.
 */
int
proc_getpgid(struct msg *msg)
{
	pid_t pid;
	struct proc *p;

	if (curproc == NULL)
		return ESRCH;

	pid = (pid_t)msg->data[0];
	if (pid == 0)
		p = curproc;
	else {
		if ((p = proc_find(pid)) == NULL)
			return ESRCH;
	}
	msg->data[0] = (int)p->p_pgrp->pg_pgid;
	DPRINTF(("proc: getpgid pgid=%d\n", msg->data[0]));
	return 0;
}

/*
 * setpgid - set process group ID for job control.
 *
 * If the specified pid is equal to 0, it the process ID of
 * the calling process is used. Also, if pgid is 0, the process
 * ID of the indicated process is used.
 */
int
proc_setpgid(struct msg *msg)
{
	pid_t pid, pgid;
	struct proc *p;
	struct pgrp *pgrp;

	DPRINTF(("proc: setpgid pid=%d pgid=%d\n", msg->data[0],
		 msg->data[1]));

	if (curproc == NULL)
		return ESRCH;

	pid = (pid_t)msg->data[0];
	if (pid == 0)
		p = curproc;
	else {
		if ((p = proc_find(pid)) == NULL)
			return ESRCH;
	}
	pgid = (pid_t)msg->data[1];
	if (pgid < 0)
		return EINVAL;
	if (pgid == 0)
		pgid = p->p_pid;
	if (p->p_pgrp->pg_pgid == pgid)	/* already leader */
		return 0;

	if ((pgrp = pgrp_find(pgid)) == NULL) {
		/*
		 * Create a new process group.
		 */
		if ((pgrp = malloc(sizeof(struct pgrp))) == NULL)
			return ENOMEM;
		list_init(&pgrp->pg_members);
		pgrp->pg_pgid = pgid;
		pgrp_add(pgrp);
	} else {
		/*
		 * Join an existing process group.
		 * Remove from the old process group.
		 */
		list_remove(&p->p_pgrp_link);
	}
	list_insert(&pgrp->pg_members, &p->p_pgrp_link);
	p->p_pgrp = pgrp;
	return 0;
}

/*
 * getsid - get the process group ID of a session leader.
 */
int
proc_getsid(struct msg *msg)
{
	pid_t pid;
	struct proc *p, *leader;

	if (curproc == NULL)
		return ESRCH;

	pid = (pid_t)msg->data[0];
	if (pid == 0)
		p = curproc;
	else {
		if ((p = proc_find(pid)) == NULL)
			return ESRCH;
	}
	leader = p->p_pgrp->pg_session->s_leader;
	msg->data[0] = (int)leader->p_pid;
	DPRINTF(("proc: getsid sid=%d\n", msg->data[0]));
	return 0;
}

/*
 * setsid - create session and set process group ID.
 */
int
proc_setsid(struct msg *msg)
{
	struct proc *p;
	struct pgrp *pgrp;
	struct session *sess;

	if (curproc == NULL)
		return ESRCH;
	p = curproc;
	if (p->p_pid == p->p_pgrp->pg_pgid)	/* already leader */
		return EPERM;

	if ((sess = malloc(sizeof(struct session))) == NULL)
		return ENOMEM;

	/*
	 * Create a new process group.
	 */
	if ((pgrp = malloc(sizeof(struct pgrp))) == NULL) {
		free(sess);
		return ENOMEM;
	}
	list_init(&pgrp->pg_members);
	pgrp->pg_pgid = p->p_pid;
	pgrp_add(pgrp);
	list_remove(&p->p_pgrp_link);
	list_insert(&pgrp->pg_members, &p->p_pgrp_link);
	p->p_pgrp = pgrp;

	/*
	 * Create a new session.
	 */
	sess->s_refcnt = 1;
	sess->s_leader = p;
	sess->s_ttyhold = 0;
	pgrp->pg_session = sess;

	msg->data[0] = (int)p->p_pid;
	DPRINTF(("proc: setsid sid=%d\n", msg->data[0]));
	return 0;
}
