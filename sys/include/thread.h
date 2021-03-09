/*-
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

#pragma once

#include <conf/config.h>
#include <context.h>
#include <timer.h>

struct as;
struct task;

/*
 * Description of a thread.
 */
struct thread {
	int		magic;		/* magic number */
	char		name[12];	/* thread name */
	struct task    *task;		/* pointer to owner task */
	list task_link;		/* link for threads in same task */
	queue link;		/* linkage on scheduling queue */
	int		state;		/* thread state */
	int		policy;		/* scheduling policy */
	int		prio;		/* current priority */
	int		baseprio;	/* base priority */
	int		timeleft;	/* remaining nanoseconds to run */
	uint_fast64_t	time;		/* total running time (nanoseconds) */
	event *slpevt;		/* sleep event */
	int		slpret;		/* sleep result code */
	timer timeout;		/* thread timer */
	k_sigset_t	sig_pending;	/* bitmap of pending signals */
	k_sigset_t	sig_blocked;	/* bitmap of blocked signals */
	void           *kstack;		/* base address of kernel stack */
	int	       *clear_child_tid;/* clear & futex_wake this on exit */
	context ctx;		/* machine specific context */
	int		errno_storage;	/* error number */
#if defined(CONFIG_DEBUG)
	int		mutex_locks;	/* mutex lock counter */
	int		spinlock_locks;	/* spinlock lock counter */
	int		rwlock_locks;	/* rwlock lock counter */
#endif
};

/*
 * Thread priorities
 */
#define PRI_TIMER	15	/* priority for timer thread */
#define PRI_IST_MAX	16	/* max priority for interrupt threads */
#define PRI_IST_MIN	32	/* min priority for interrupt threads */
#define PRI_DPC		33	/* priority for Deferred Procedure Call */
#define PRI_KERN_HIGH	34	/* high priority kernel threads */
#define PRI_KERN_LOW	35	/* low priority kernel threads */
#define PRI_USER_MAX	150	/* maximum user thread priority */
#define PRI_DEFAULT	200	/* default user priority */
#define PRI_USER_MIN	250	/* minimum user thread priority */
#define PRI_BACKGROUND	254	/* priority for background processing */
#define PRI_IDLE	255	/* priority for idle thread */
#define PRI_MIN		255	/* minimum priority */

/*
 * Thread state
 */
#define TH_SLEEP	0x01	/* awaiting an event */
#define TH_SUSPEND	0x02	/* suspended */
#define TH_EXIT		0x04	/* terminating */
#define TH_ZOMBIE	0x08	/* dead */
#define TH_U_ACCESS	0x10	/* thread is accessing userspace */
#define TH_U_ACCESS_S	0x20	/* thread access to userspace was suspended */

/*
 * Normal threads
 */
thread  *thread_cur();
bool		thread_valid(thread *);
int thread_createfor(task *, as *, thread **, void *, long, void (*)(), long);
int	        thread_name(thread *, const char *);
int		thread_id(thread *);
thread  *thread_find(int);
void	        thread_terminate(thread *);
void		thread_zombie(thread *);
[[noreturn]] void thread_idle();
void	        thread_dump();
void	        thread_check();
void	        thread_init();

/*
 * Kernel threads
 */
thread *kthread_create(void (*)(void *), void *, int, const char *, long);
