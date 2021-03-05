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

#pragma once

#include <csignal>
#include <futex.h>
#include <ksigaction.h>
#include <sync.h>
#include <timer.h>

/*
 * Task struct
 */
struct task {
	int		magic;		    /* magic number */
	char	       *path;		    /* path to executable */
	struct list	link;		    /* link for all tasks in system */
	struct list	threads;	    /* threads in this task */
	struct as      *as;		    /* address space description */
	int		suscnt;		    /* suspend counter */
	unsigned	capability;	    /* security permission flag */
	struct task    *parent;		    /* parent task */
	struct futexes	futexes;	    /* futex state for task */
	struct itimer	itimer_prof;	    /* interval timer ITIMER_PROF */
	struct itimer	itimer_virtual;	    /* interval timer ITIMER_VIRTUAL */
	struct timer	itimer_real;	    /* interval timer ITIMER_REAL */

	/* Signal Management */
	k_sigset_t	    sig_pending;	/* pending signals */
	struct k_sigaction  sig_action[NSIG];	/* signal handlers */

	/* Process Management */
	pid_t		pgid;		    /* process group id */
	pid_t		sid;		    /* session id */
	int	        state;		    /* process state */
	int	        exitcode;	    /* process exit code */
	struct event	child_event;	    /* child exited event */
	int		termsig;	    /* signal to parent on terminate */
	struct thread  *vfork;		    /* vfork thread to wake */
	struct event	thread_event;	    /* thread exited event */

	/* File System State */
	struct rwlock	fs_lock;	    /* lock for file system data */
	uintptr_t	file[64];	    /* array of file pointers */
	struct file    *cwdfp;		    /* directory for cwd */
	mode_t		umask;		    /* current file creation mask */
};

/* process status */
#define PS_RUN		1		    /* running */
#define PS_ZOMB		2		    /* terminated but not waited for */
#define PS_STOP		3		    /* stopped */

/* vm option for task_create(). */
#define VM_NEW		0		    /* create new memory map */
#define VM_SHARE	1		    /* share parent's memory map */
#define VM_COPY		2		    /* duplicate parent's memory map */

/* capabilities */
#define CAP_SETPCAP	0x00000001  /* setting capability */
#define CAP_TASK	0x00000002  /* controlling another task's execution */
#define CAP_MEMORY	0x00000004  /* touching another task's memory */
#define CAP_KILL	0x00000008  /* raising exception to another task */
#define CAP_SEMAPHORE	0x00000010  /* accessing another task's semaphore */
#define CAP_NICE	0x00000020  /* changing scheduling parameter */
#define CAP_IPC		0x00000040  /* accessing another task's IPC object */
#define CAP_DEVIO	0x00000080  /* device I/O operation */
#define CAP_POWER	0x00000100  /* power control including shutdown */
#define CAP_TIME	0x00000200  /* setting system time */
#define CAP_RAWIO	0x00000400  /* direct I/O access */
#define CAP_DEBUG	0x00000800  /* debugging requests */
#define CAP_ADMIN	0x00010000  /* mount,umount,sethostname,setdomainname,etc */


struct task    *task_cur();
struct task    *task_find(pid_t);
pid_t		task_pid(struct task *);
bool		task_valid(struct task *);
int		task_create(struct task *, int, struct task **);
int		task_destroy(struct task *);
int	        task_suspend(struct task *);
int	        task_resume(struct task *);
int	        task_path(struct task *, const char *);
bool	        task_capable(unsigned);
bool	        task_access(struct task *);
struct futexes *task_futexes(struct task *);
void		task_dump();
void		task_init();
