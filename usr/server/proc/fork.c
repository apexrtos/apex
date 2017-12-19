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
 * fork.c - fork() support
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "proc.h"

static int vfork_start(struct proc *);

/*
 * fork() support.
 *
 * It creates new process data and update all process relations.
 * The task creation and the thread creation are done by the
 * fork() library stub.
 */
int
proc_fork(struct msg *msg)
{
	struct proc *p;
	struct pgrp *pgrp;
	task_t child;
	pid_t pid;
	int vfork_flag;

	if (curproc == NULL)
		return EINVAL;

	child = (task_t)msg->data[0];
	vfork_flag = msg->data[1];

	DPRINTF(("fork: parent=%x child=%x vfork=%d\n", msg->hdr.task,
		 child, vfork_flag));

	if (task_to_proc(child) != NULL)
		return EINVAL;	/* Process already exists */

	if ((pid = pid_assign()) == 0)
		return EAGAIN;	/* Too many processes */

	if ((p = malloc(sizeof(struct proc))) == NULL)
		return ENOMEM;

	p->p_parent = curproc;
	p->p_pgrp = curproc->p_pgrp;
	p->p_stat = SRUN;
	p->p_exitcode = 0;
	p->p_pid = pid;
	p->p_task = child;
	list_init(&p->p_children);
	proc_add(p);
	list_insert(&curproc->p_children, &p->p_sibling);
	pgrp = p->p_pgrp;
	list_insert(&pgrp->pg_members, &p->p_pgrp_link);
	list_insert(&allproc, &p->p_link);

	if (vfork_flag)
		vfork_start(curproc);

	DPRINTF(("fork: new pid=%d\n", p->p_pid));
	msg->data[0] = (int)p->p_pid;
	return 0;
}

/*
 * Clean up all resource created by fork().
 */
void
proc_cleanup(struct proc *p)
{
	struct proc *pp;

	pp = p->p_parent;
	list_remove(&p->p_sibling);
	list_remove(&p->p_pgrp_link);
	proc_remove(p);
	list_remove(&p->p_link);
	free(p);
}

static int
vfork_start(struct proc *p)
{
	void *stack;

	/*
	 * Save parent's stack
	 */
	if (vm_allocate(p->p_task, &stack, USTACK_SIZE, 1) != 0)
		return ENOMEM;

	memcpy(stack, p->p_stackbase, USTACK_SIZE);
	p->p_stacksaved = stack;

	p->p_vforked = 1;
	DPRINTF(("vfork_start: saved=%x org=%x\n", stack, p->p_stackbase));
	return 0;
}

void
vfork_end(struct proc *p)
{

	DPRINTF(("vfork_end: org=%x saved=%x\n", p->p_stackbase,
		 p->p_stacksaved));
	/*
	 * Restore parent's stack
	 */
	memcpy(p->p_stackbase, p->p_stacksaved, USTACK_SIZE);
	vm_free(p->p_task, p->p_stacksaved);

	/*
	 * Resume parent
	 */
	p->p_vforked = 0;
	task_resume(p->p_task);
}
