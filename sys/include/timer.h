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

#pragma once

#include <event.h>
#include <list.h>
#include <types.h>

struct timespec32;
struct timespec;
struct timeval;

struct timer {
	list link;		/* linkage on timer chain */
	int		active;		/* true if active */
	uint_fast64_t	expire;		/* expire time (nsec) */
	uint_fast64_t	interval;	/* time interval (nsec) */
	void	      (*func)(void *);	/* function to call */
	void	       *arg;		/* function argument */
};

struct itimer {
	uint_fast64_t remain;		/* remaining time, 0 if disabled */
	uint_fast64_t interval;		/* reload interval, 0 if disabled */
};


uint_fast64_t ts_to_ns(const timespec &);
uint_fast64_t ts32_to_ns(const timespec32 &);
timespec ns_to_ts(uint_fast64_t);
timespec32 ns_to_ts32(uint_fast64_t);
uint_fast64_t tv_to_ns(const timeval &);
timeval ns_to_tv(uint_fast64_t);

void	      timer_callout(timer *, uint_fast64_t, uint_fast64_t,
			  void (*)(void *), void *);
void	      timer_redirect(timer *, void (*)(void *), void *);
void	      timer_stop(timer *);
uint_fast64_t timer_delay(uint_fast64_t);
void	      timer_tick(int);
uint_fast64_t timer_monotonic();
uint_fast64_t timer_monotonic_coarse();
int	      timer_realtime_set(uint_fast64_t);
uint_fast64_t timer_realtime();
uint_fast64_t timer_realtime_coarse();
void	      timer_init();
