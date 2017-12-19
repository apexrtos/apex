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

#ifndef _TIMER_H
#define _TIMER_H

#include <sys/cdefs.h>
#include <event.h>

struct timer {
	struct list	link;		/* linkage on timer chain */
	int		active;		/* true if active */
	u_long		expire;		/* expire time (ticks) */
	u_long		interval;	/* time interval */
	struct event	event;		/* event for this timer */
	void (*func)(void *);		/* function to call */
	void		*arg;		/* function argument */
};

/*
 * Macro to compare two timer counts.
 * time_after() returns true if a is after b.
 */
#define time_after(a,b)		(((long)(b) - (long)(a)) < 0)
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)	(((long)(a) - (long)(b)) >= 0)
#define time_before_eq(a,b)	time_after_eq(b,a)

/*
 * Macro to convert milliseconds and tick.
 */
#define msec_to_tick(ms)	(((ms) >= 0x20000) ? \
				 ((ms) / 1000UL) * HZ : ((ms) * HZ) / 1000UL)

#define tick_to_msec(tick)	(((tick) * 1000) / HZ)


__BEGIN_DECLS
void	 timer_callout(struct timer *, u_long, void (*)(void *), void *);
void	 timer_stop(struct timer *);
u_long	 timer_delay(u_long);
int	 timer_sleep(u_long, u_long *);
int	 timer_alarm(u_long, u_long *);
int	 timer_periodic(struct thread *, u_long, u_long);
int	 timer_waitperiod(void);
void	 timer_cleanup(struct thread *);
int	 timer_hook(void (*)(int));
void	 timer_tick(void);
u_long	 timer_count(void);
void	 timer_info(struct info_timer *);
void	 timer_init(void);
__END_DECLS

#endif /* !_TIMER_H */
