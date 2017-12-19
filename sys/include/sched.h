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

#ifndef _SCHED_H
#define _SCHED_H

#include <sys/cdefs.h>
#include <event.h>
#include <thread.h>

/*
 * Scheduling policies defined by IEEE Std 1003.1-2001
 */
#define SCHED_FIFO	0	/* First in-first out */
#define SCHED_RR	1	/* Round robin */
#define	SCHED_OTHER	2	/* Another scheduling policy */

/*
 * Scheduling quantum (Ticks for context switch)
 */
#define QUANTUM		(CONFIG_TIME_SLICE * HZ / 1000)

/*
 * DPC (Deferred Procedure Call) object
 */
struct dpc {
	struct queue	link;		/* Linkage on DPC queue */
	int		state;
	void		(*func)(void *); /* Callback routine */
	void		*arg;		/* Argument to pass */
};

/* state for DPC */
#define DPC_FREE	0x4470463f	/* 'DpF?' */
#define DPC_PENDING	0x4470503f	/* 'DpP?' */

#define sched_sleep(evt)	sched_tsleep((evt), 0)

__BEGIN_DECLS
int	 sched_tsleep(struct event *, u_long);
void	 sched_wakeup(struct event *);
thread_t sched_wakeone(struct event *);
void	 sched_unsleep(thread_t, int);
void	 sched_yield(void);
void	 sched_suspend(thread_t);
void	 sched_resume(thread_t);
void	 sched_tick(void);
void	 sched_start(thread_t);
void	 sched_stop(thread_t);
void	 sched_lock(void);
void	 sched_unlock(void);
int	 sched_getprio(thread_t);
void	 sched_setprio(thread_t, int, int);
int	 sched_getpolicy(thread_t);
int	 sched_setpolicy(thread_t, int);
void	 sched_dpc(struct dpc *, void (*)(void *), void *);
void	 sched_init(void);
__END_DECLS

#endif /* !_SCHED_H */
