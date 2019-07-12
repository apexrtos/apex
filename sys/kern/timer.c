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

/*
 * timer.c - kernel timer services.
 *
 * TODO:
 * - monotonic should not be volatile
 * - replace irq_disable/irq_restore with spinlock (required for SMP)
 */

#include <timer.h>

#include <access.h>
#include <arch.h>
#include <assert.h>
#include <clock.h>
#include <debug.h>
#include <errno.h>
#include <irq.h>
#include <sch.h>
#include <sections.h>
#include <sig.h>
#include <stdlib.h>	    /* remove when lldiv is no longer required */
#include <sys/mman.h>
#include <sys/time.h>
#include <task.h>
#include <thread.h>

static volatile uint64_t monotonic __fast_bss;	/* nanoseconds elapsed since bootup */

static struct event	timer_event;	/* event to wakeup a timer thread */
static struct event	delay_event;	/* event for the thread delay */
static struct list	timer_list;	/* list of active timers */
static struct list	expire_list;	/* list of expired timers */

/*
 * Get remaining nanoseconds to the expiration time.
 * Return 0 if time already passed.
 */
static uint64_t
time_remain(uint64_t expire)
{
	if (expire < monotonic)
		return expire - monotonic;
	return 0;
}

/*
 * Insert a timer element into the timer list in the proper place.
 * Requires interrupts to be disabled by the caller.
 */
static void
timer_insert(struct timer *tmr)
{
	struct list *head, *n;
	struct timer *t;

	/*
	 * We sort the timer list by time. So, we can
	 * quickly get the next expiration time from
	 * the head element of the timer list.
	 */
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		if (tmr->expire < t->expire)
			break;
	}
	list_insert(list_prev(n), &tmr->link);
}

/*
 * Convert from timespec to nanoseconds
 */
uint64_t
ts_to_ns(const struct timespec *ts)
{
	return ts->tv_sec * 1000000000ULL + ts->tv_nsec;
}

/*
 * Convert from nanoseconds to timespec
 */
void
ns_to_ts(uint64_t ns, struct timespec *ts)
{
	/* REVISIT: this crap shouldn't be required, but:
	   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86011 */
#if UINTPTR_MAX == 0xffffffff
	const lldiv_t res = lldiv(ns, 1000000000);
	ts->tv_sec = res.quot;
	ts->tv_nsec = res.rem;
#else
	ts->tv_sec = ns / 1000000000ULL;
	ts->tv_nsec = ns % 1000000000ULL;
#endif
}

/*
 * Convert from timeval to nanoseconds
 */
uint64_t
tv_to_ns(const struct timeval *tv)
{
	return tv->tv_sec * 1000000000ULL + tv->tv_usec * 1000ULL;
}

/*
 * Convert from nanoseconds to timeval
 */
void
ns_to_tv(uint64_t ns, struct timeval *tv)
{
	/* REVISIT: this crap shouldn't be required, but:
	   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86011 */
#if UINTPTR_MAX == 0xffffffff
	const lldiv_t res = lldiv(ns, 1000000000);
	tv->tv_sec = res.quot;
	tv->tv_usec = (uint32_t)res.rem / 1000;
#else
	tv->tv_sec = ns / 1000000000;
	tv->tv_usec = (uint32_t)(ns % 1000000000) / 1000;
#endif
}

/*
 * Schedule a callout function to run after a specified
 * length of time.  A device driver can call
 * timer_callout()/timer_stop() from ISR at interrupt level.
 *
 * If nsec == 1 we call out after the next tick.
 */
void
timer_callout(struct timer *tmr, uint64_t nsec, uint64_t interval,
	      void (*func)(void *), void *arg)
{
	assert(tmr);

	const uint64_t period = 1000000000 / CONFIG_HZ;

	const int s = irq_disable();
	if (tmr->active)
		list_remove(&tmr->link);
	tmr->func = func;
	tmr->arg = arg;
	tmr->active = 1;
	tmr->interval = interval;
	/*
	 * Guarantee that we will call out after at least nsec.
	 */
	tmr->expire = monotonic + period + (nsec == 1 ? 0 : nsec);
	timer_insert(tmr);
	irq_restore(s);
}

/*
 * if timer is active adjust callback function
 */
void
timer_redirect(struct timer *tmr, void (func)(void *), void *arg)
{
	const int s = irq_disable();
	if (tmr->active) {
		tmr->func = func;
		tmr->arg = arg;
	}
	irq_restore(s);
}

/*
 * stop timer
 */
void
timer_stop(struct timer *tmr)
{
	assert(tmr);

	const int s = irq_disable();
	if (tmr->active) {
		list_remove(&tmr->link);
		tmr->active = 0;
	}
	irq_restore(s);
}

/*
 * timer_delay - delay thread execution.
 *
 * The caller thread is blocked for the specified time.
 * Returns 0 on success, or the remaining time (nsec) on failure.
 * This service is not available at interrupt level.
 *
 * If nsec <= 1 we delay until the next tick.
 */
uint64_t
timer_delay(uint64_t nsec)
{
	struct timer *tmr;
	u_long remain = 0;
	int rc;

	if ((rc = sch_prepare_sleep(&delay_event, nsec ?: 1)) == 0)
		rc = sch_continue_sleep();

	if (rc != -ETIMEDOUT) {
		tmr = &thread_cur()->timeout;
		remain = time_remain(tmr->expire);
	}
	return remain;
}

/*
 * Timer thread.
 *
 * Handle all expired timers. Each callout routine is
 * called with scheduler locked and interrupts enabled.
 */
static void
timer_thread(void *arg)
{
	struct timer *tmr;

	for (;;) {
		/* Wait until next timer expiration. */
		interrupt_disable();
		sch_prepare_sleep(&timer_event, 0);
		interrupt_enable();
		sch_continue_sleep();
		interrupt_disable();

		while (!list_empty(&expire_list)) {
			/*
			 * Callout
			 */
			tmr = list_entry(list_first(&expire_list),
					 struct timer, link);
			list_remove(&tmr->link);

			if (tmr->interval != 0) {
				/*
				 * Periodic timer
				 */
				tmr->expire += tmr->interval;
				timer_insert(tmr);
			} else {
				/*
				 * One-shot timer
				 */
				tmr->active = 0;
			}

			interrupt_enable();
			(*tmr->func)(tmr->arg);
			interrupt_disable();
		}
	}
	/* NOTREACHED */
}

/*
 * run_itimer - decrement elapsed time from itimer, signal if time expired,
 * reload if configured to do so.
 */
__fast_text static void
run_itimer(struct itimer *it, uint32_t ns, int sig)
{
	/* disabled */
	if (!it->remain)
		return;

	/* not yet expired */
	if (ns < it->remain) {
		it->remain -= ns;
		return;
	}

	/* expired */
	it->remain = it->interval - (ns - it->remain);
	if (it->remain > it->interval)
		it->remain = 1;	    /* overflow */
	sig_task(task_cur(), sig);
}

/*
 * Timer tick handler
 *
 * timer_tick() is called straight from the real time clock interrupt.
 */
__fast_text void
timer_tick(int ticks)
{
	struct timer *tmr;
	int wakeup = 0;
	struct task *t = task_cur();

	/*
	 * Convert elapsed time to nanoseconds
	 */
	const uint32_t ns = ticks * 1000000000 / CONFIG_HZ;

	/*
	 * Bump time.
	 */
	monotonic += ns;

	/*
	 * Handle all of the timer elements that have expired.
	 */
	while (!list_empty(&timer_list)) {
		/*
		 * Check timer expiration.
		 */
		tmr = list_entry(list_first(&timer_list), struct timer, link);
		if (monotonic < tmr->expire)
			break;
		/*
		 * Move expired timers from timer list to expire list.
		 */
		list_remove(&tmr->link);
		list_insert(&expire_list, &tmr->link);
		wakeup = 1;
	}
	if (wakeup)
		sch_wakeup(&timer_event, 0);

	/* itimer_prof decrements any time the process is running */
	run_itimer(&t->itimer_prof, ns, SIGPROF);

	/* itimer_virtual decrements only when the process is in userspace */
	if (interrupt_from_userspace())
		run_itimer(&t->itimer_virtual, ns, SIGVTALRM);

	sch_elapse(ns);
}

/*
 * Return monotonic time
 */
uint64_t
timer_monotonic(void)
{
	const int s = irq_disable();
	const uint64_t r = monotonic + clock_ns_since_tick();
	irq_restore(s);
	return r;
}

/*
 * Return monotonic time (coarse, fast version)
 */
uint64_t
timer_monotonic_coarse(void)
{
	return monotonic;
}

/*
 * Initialize the timer facility, called at system startup time.
 */
void
timer_init(void)
{
	struct thread *th;

	list_init(&timer_list);
	list_init(&expire_list);
	event_init(&timer_event, "timer", ev_SLEEP);
	event_init(&delay_event, "delay", ev_SLEEP);

	/* Start timer thread */
	th = kthread_create(&timer_thread, NULL, PRI_TIMER, "timer", MA_FAST);
	if (th == NULL)
		panic("timer_init");
}

/*
 * sc_getitimer - get the value of an interval timer
 */
int
sc_getitimer(int timer, struct itimerval *o)
{
	int err;

	if (timer < 0 || timer > ITIMER_PROF)
		return DERR(-EINVAL);

	if ((err = u_access_begin()) < 0)
		return err;
	if (!u_access_ok(o, sizeof *o, PROT_WRITE)) {
		u_access_end();
		return DERR(-EFAULT);
	}

	struct task *t = task_cur();
	const int s = irq_disable();

	switch (timer) {
	case ITIMER_PROF:
		ns_to_tv(t->itimer_prof.remain, &o->it_value);
		ns_to_tv(t->itimer_prof.interval, &o->it_interval);
		break;
	case ITIMER_VIRTUAL:
		ns_to_tv(t->itimer_virtual.remain, &o->it_value);
		ns_to_tv(t->itimer_virtual.interval, &o->it_interval);
		break;
	case ITIMER_REAL:
		ns_to_tv(time_remain(t->itimer_real.expire), &o->it_value);
		ns_to_tv(t->itimer_real.interval, &o->it_interval);
		break;
	}

	irq_restore(s);
	u_access_end();

	return 0;
}

/*
 * sc_setitimer - set the value of an interval timer
 */
int
sc_setitimer(int timer, const struct itimerval *n, struct itimerval *o)
{
	if (timer < 0 || timer > ITIMER_PROF)
		return DERR(-EINVAL);

	u_access_begin();
	if (!u_access_ok(n, sizeof *n, PROT_READ) ||
	    (o && !u_access_ok(o, sizeof *o, PROT_WRITE))) {
		u_access_end();
		return DERR(-EFAULT);
	}

	void alarm(void *tv)
	{
		sig_task((struct task *)tv, SIGALRM);
	}

	struct itimerval old;
	struct task *t = task_cur();
	const int s = irq_disable();

	switch (timer) {
	case ITIMER_PROF:
		if (o) {
			ns_to_tv(t->itimer_prof.remain, &old.it_value);
			ns_to_tv(t->itimer_prof.interval, &old.it_interval);
		}
		t->itimer_prof.remain = tv_to_ns(&n->it_value);
		t->itimer_prof.interval = tv_to_ns(&n->it_interval);
		break;
	case ITIMER_VIRTUAL:
		if (o) {
			ns_to_tv(t->itimer_virtual.remain, &old.it_value);
			ns_to_tv(t->itimer_virtual.interval, &old.it_interval);
		}
		t->itimer_virtual.remain = tv_to_ns(&n->it_value);
		t->itimer_virtual.interval = tv_to_ns(&n->it_interval);
		break;
	case ITIMER_REAL:
		if (o) {
			ns_to_tv(time_remain(t->itimer_real.expire), &old.it_value);
			ns_to_tv(t->itimer_real.interval, &old.it_interval);
		}
		const u_long ns = tv_to_ns(&n->it_value);
		if (!ns)
			timer_stop(&t->itimer_real);
		else
			timer_callout(&t->itimer_real, ns,
			    tv_to_ns(&n->it_interval), alarm, t);
		break;
	}

	irq_restore(s);

	if (o)
		*o = old;

	u_access_end();

	return 0;
}
