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
 * hash.c - pid/pgid mapping tables.
 */

#include <server/stdmsg.h>
#include <sys/list.h>
#include <unistd.h>

#include "proc.h"

/*
 * Hash tables for ID mapping
 */
static struct list pid_hash[ID_MAXBUCKETS];	/* mapping: pid  -> proc */
static struct list task_hash[ID_MAXBUCKETS];	/* mapping: task -> proc */
static struct list pgid_hash[ID_MAXBUCKETS];	/* mapping: pgid -> pgrp */

/*
 * Find process by pid.
 */
struct proc *
proc_find(pid_t pid)
{
	list_t head, n;
	struct proc *p = NULL;

	head = &pid_hash[IDHASH(pid)];
	n = list_first(head);
	while (n != head) {
		p = list_entry(n, struct proc, p_pid_link);
		if (p->p_pid == pid)
			return p;
		n = list_next(n);
	}
	return NULL;
}

/*
 * Find process group by pgid.
 */
struct pgrp *
pgrp_find(pid_t pgid)
{
	list_t head, n;
	struct pgrp *g = NULL;

	head = &pgid_hash[IDHASH(pgid)];
	n = list_first(head);
	while (n != head) {
		g = list_entry(n, struct pgrp, pg_link);
		if (g->pg_pgid == pgid)
			return g;
		n = list_next(n);
	}
	return NULL;
}

/*
 * Find process by task ID.
 */
struct proc *
task_to_proc(task_t task)
{
	list_t head, n;
	struct proc *p = NULL;

	head = &task_hash[IDHASH(task)];
	n = list_first(head);

	while (n != head) {
		p = list_entry(n, struct proc, p_task_link);
		if (p->p_task == task)
			return p;
		n = list_next(n);
	}
	return NULL;
}

/*
 * Add process to the pid table and the task table.
 * This routine assumes pid and task data has been already initialized.
 */
void
proc_add(struct proc *p)
{

	list_insert(&pid_hash[IDHASH(p->p_pid)], &p->p_pid_link);
	list_insert(&task_hash[IDHASH(p->p_task)], &p->p_task_link);
}

/*
 * Remove process from both of pid table and task table.
 */
void
proc_remove(struct proc *p)
{

	list_remove(&p->p_pid_link);
	list_remove(&p->p_task_link);
}

/*
 * Add process group to table.
 * This routine assumes pgid has been already initialized.
 */
void
pgrp_add(struct pgrp *pgrp)
{

	list_insert(&pgid_hash[IDHASH(pgrp->pg_pgid)], &pgrp->pg_link);
}

/*
 * Remove process from pgid table.
 */
void
pgrp_remove(struct pgrp *pgrp)
{

	list_remove(&pgrp->pg_link);
}

/*
 * Initialize tables.
 */
void
table_init(void)
{
	int i;

	for (i = 0; i < ID_MAXBUCKETS; i++) {
		list_init(&pid_hash[i]);
		list_init(&task_hash[i]);
		list_init(&pgid_hash[i]);
	}
}
