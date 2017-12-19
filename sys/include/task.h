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

#ifndef _TASK_H
#define _TASK_H

#include <sys/cdefs.h>
#include <prex/capability.h>	/* for cap_t */
#include <timer.h>

/*
 * Task struct
 */
struct task {
	int		magic;		/* magic number */
	char		name[MAXTASKNAME]; /* task name */
	struct list	link;		/* link for all tasks in system */
	struct list	objects;	/* objects owned by this task */
	struct list	threads;	/* threads in this task */
	vm_map_t	map;		/* address space description */
	int		suscnt;		/* suspend counter */
	cap_t		capability;	/* security permission flag */
	struct timer	alarm;		/* alarm timer */
	void (*handler)(int);		/* pointer to exception handler */
	task_t		parent;		/* parent task */
};

#define cur_task()	  (cur_thread->task)
#define task_valid(tsk)	  (kern_area(tsk) && ((tsk)->magic == TASK_MAGIC))

/* vm option for task_create(). */
#define VM_NEW		0	/* Create new memory map */
#define VM_SHARE	1	/* Share parent's memory map */
#define VM_COPY		2	/* Duplicate parent's memory map */


__BEGIN_DECLS
int	 task_create(task_t, int, task_t *);
int	 task_terminate(task_t);
task_t	 task_self(void);
int	 task_suspend(task_t);
int	 task_resume(task_t);
int	 task_name(task_t, const char *);
int	 task_getcap(task_t, cap_t *);
int	 task_setcap(task_t, cap_t *);
int	 task_capable(cap_t);
int	 task_access(task_t);
void	 task_bootstrap(void);
void	 task_dump(void);
void	 task_init(void);
__END_DECLS

#endif /* !_TASK_H */
