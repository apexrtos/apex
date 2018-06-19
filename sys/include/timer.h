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

#ifndef timer_h
#define timer_h

#include <event.h>
#include <list.h>
#include <types.h>

struct itimerval;
struct timespec;
struct timeval;

struct timer {
	struct list	link;		/* linkage on timer chain */
	int		active;		/* true if active */
	uint64_t	expire;		/* expire time (nsec) */
	uint64_t	interval;	/* time interval (nsec) */
	struct event	event;		/* event for this timer */
	void	      (*func)(void *);	/* function to call */
	void	       *arg;		/* function argument */
};

struct itimer {
	uint64_t remain;		/* remaining time, 0 if disabled */
	uint64_t interval;		/* reload interval, 0 if disabled */
};

uint64_t    ts_to_ns(const struct timespec *);
void	    ns_to_ts(uint64_t, struct timespec *);
uint64_t    tv_to_ns(const struct timeval *);
void	    ns_to_tv(uint64_t, struct timeval *);

void	    timer_callout(struct timer *, uint64_t, uint64_t, void (*)(void *), void *);
void	    timer_redirect(struct timer *, void (*)(void *), void *);
void	    timer_stop(struct timer *);
uint64_t    timer_delay(uint64_t);
void	    timer_tick(int);
uint64_t    timer_monotonic(void);
void	    timer_init(void);

/*
 * Syscalls
 */
int	    sc_getitimer(int, struct itimerval *);
int	    sc_setitimer(int, const struct itimerval *, struct itimerval *);

#endif /* !timer_h */
