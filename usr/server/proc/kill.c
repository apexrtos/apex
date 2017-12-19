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
 * kill.c - signal transfer.
 */

#include <prex/prex.h>
#include <prex/capability.h>
#include <server/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>

#include "proc.h"

/*
 * Send a signal to the process.
 */
static int
send_sig(struct proc *p, int sig)
{

	if (p->p_pid == 0 || p->p_pid == 1)
		return EPERM;

	DPRINTF(("proc: send_sig task=%x\n", p->p_task));
	return exception_raise(p->p_task, sig);
}

/*
 * Send a signal to one process.
 */
static int
kill_one(pid_t pid, int sig)
{
	struct proc *p;

	DPRINTF(("proc: killone pid=%d sig=%d\n", pid, sig));

	if ((p = proc_find(pid)) == NULL)
		return ESRCH;
	return send_sig(p, sig);
}

/*
 * Send a signal to all process in the process group.
 */
int
kill_pg(pid_t pgid, int sig)
{
	struct proc *p;
	struct pgrp *pgrp;
	list_t head, n;
	int err = 0;

	DPRINTF(("proc: killpg pgid=%d sig=%d\n", pgid, sig));

	if ((pgrp = pgrp_find(pgid)) == NULL)
		return ESRCH;

	head = &pgrp->pg_members;
	for (n = list_first(head); n != head; n = list_next(n)) {
		p = list_entry(n, struct proc, p_link);
		if ((err = send_sig(p, sig)) != 0)
			break;
	}
	return err;
}

/*
 * Send a signal.
 *
 * The behavior is different for the pid value.
 *
 *  if (pid > 0)
 *    Send a signal to specific process.
 *
 *  if (pid == 0)
 *    Send a signal to all processes in same process group.
 *
 *  if (pid == -1)
 *    Send a signal to all processes except init.
 *
 *  if (pid < -1)
 *     Send a signal to the process group.
 *
 * Note: Need CAP_KILL capability to send a signal to the different
 * process/group.
 */
int
proc_kill(struct msg *msg)
{
	pid_t pid;
	struct proc *p;
	list_t n;
	int sig, capable = 0;
	int err = 0;

	pid = (pid_t)msg->data[0];
	sig = msg->data[1];

	DPRINTF(("proc: kill pid=%d sig=%d\n", pid, sig));

	switch (sig) {
	case SIGFPE:
	case SIGILL:
	case SIGSEGV:
		return EINVAL;
	}

	if (curproc->p_cap & CAP_KILL)
		capable = 1;

	if (pid > 0) {
		if (pid != curproc->p_pid && !capable)
			return EPERM;
		err = kill_one(pid, sig);
	}
	else if (pid == -1) {
		if (!capable)
			return EPERM;
		for (n = list_first(&allproc); n != &allproc;
		     n = list_next(n)) {
			p = list_entry(n, struct proc, p_link);
			if (p->p_pid != 0 && p->p_pid != 1) {
				err = kill_one(p->p_pid, sig);
				if (err != 0)
					break;
			}
		}
	}
	else if (pid == 0) {
		if ((p = proc_find(pid)) == NULL)
			return ESRCH;
		err = kill_pg(p->p_pgrp->pg_pgid, sig);
	}
	else {
		if (curproc->p_pgrp->pg_pgid != -pid && !capable)
			return EPERM;
		err = kill_pg(-pid, sig);
	}
	return err;
}
