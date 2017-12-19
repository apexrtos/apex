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
 */

#include <kernel.h>
#include <task.h>
#include <event.h>
#include <timer.h>
#include <irq.h>
#include <sched.h>
#include <thread.h>
#include <kmem.h>
#include <exception.h>

static volatile u_long	lbolt;		/* ticks elapsed since bootup */

static struct event	timer_event;	/* event to wakeup a timer thread */
static struct event	delay_event;	/* event for the thread delay */
static struct list	timer_list;	/* list of active timers */
static struct list	expire_list;	/* list of expired timers */

static void (*volatile tick_hook)(int); /* hook routine for timer tick */

/*
 * Macro to get a timer element for the next expiration.
 */
#define timer_next() \
	(list_entry(list_first(&timer_list), struct timer, link))

/*
 * Get remaining ticks to the expiration time.
 * Return 0 if time already passed.
 */
static u_long
time_remain(u_long expire)
{

	if (time_before(lbolt, expire))
		return expire - lbolt;
	return 0;
}

/*
 * Add a timer element to the timer list in the proper place.
 * Requires interrupts to be disabled by the caller.
 */
static void
timer_add(struct timer *tmr, u_long ticks)
{
	list_t head, n;
	struct timer *t;

	tmr->expire = lbolt + ticks;

	/*
	 * We sort the timer list by time. So, we can
	 * quickly get the next expiration time from
	 * the head element of the timer list.
	 */
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		if (time_before(tmr->expire, t->expire))
			break;
	}
	list_insert(list_prev(n), &tmr->link);
}

/*
 * Schedule a callout function to run after a specified
 * length of time.  A device driver can call
 * timer_callout()/timer_stop() from ISR at interrupt level.
 */
void
timer_callout(struct timer *tmr, u_long msec,
	      void (*func)(void *), void *arg)
{
	u_long ticks;

	ASSERT(tmr);

	ticks = msec_to_tick(msec);
	if (ticks == 0)
		ticks = 1;

	irq_lock();
	if (tmr->active)
		list_remove(&tmr->link);
	tmr->func = func;
	tmr->arg = arg;
	tmr->active = 1;
	tmr->interval = 0;
	timer_add(tmr, ticks);
	irq_unlock();
}

void
timer_stop(struct timer *tmr)
{
	ASSERT(tmr);

	irq_lock();
	if (tmr->active) {
		list_remove(&tmr->link);
		tmr->active = 0;
	}
	irq_unlock();
}

/*
 * timer_delay - delay thread execution.
 *
 * The caller thread is blocked for the specified time.
 * Returns 0 on success, or the remaining time (msec) on failure.
 * This service is not available at interrupt level.
 */
u_long
timer_delay(u_long msec)
{
	struct timer *tmr;
	u_long remain = 0;
	int rc;

	ASSERT(irq_level == 0);

	rc = sched_tsleep(&delay_event, msec);
	if (rc != SLP_TIMEOUT) {
		tmr = &cur_thread->timeout;
		remain = tick_to_msec(time_remain(tmr->expire));
	}
	return remain;
}

/*
 * timer_sleep - sleep system call.
 *
 * Stop execution of the current thread for the indicated amount
 * of time.  If the sleep is interrupted, the remaining time
 * is set in "remain".  Returns EINTR if sleep is canceled by
 * some reasons.
 */
int
timer_sleep(u_long msec, u_long *remain)
{
	u_long left;
	int err = 0;

	left = timer_delay(msec);

	if (remain != NULL)
		err = umem_copyout(&left, remain, sizeof(left));
	if (err == 0 && left > 0)
		err = EINTR;
	return err;
}

/*
 * Alarm timer expired:
 * Send an alarm exception to the target task.
 */
static void
alarm_expire(void *arg)
{

	exception_post((task_t)arg, SIGALRM);
}

/*
 * timer_alarm - alarm system call.
 *
 * SIGALRM exception is sent to the caller task when specified
 * delay time is passed. If passed time is 0, stop the current
 * running timer.
 */
int
timer_alarm(u_long msec, u_long *remain)
{
	struct timer *tmr;
	task_t self = cur_task();
	u_long left = 0;
	int err = 0;

	irq_lock();
	tmr = &self->alarm;
	if (tmr->active) {
		/*
		 * Save the remaining time to return
		 * before we update the timer value.
		 */
		left = tick_to_msec(time_remain(tmr->expire));
	}
	if (msec == 0)
		timer_stop(tmr);
	else
		timer_callout(tmr, msec, &alarm_expire, self);

	irq_unlock();

	if (remain != NULL)
		err = umem_copyout(&left, remain, sizeof(left));
	return err;
}

/*
 * timer_periodic - set periodic timer for the specified thread.
 *
 * The periodic thread will wait the timer period by calling
 * timer_waitperiod(). The unit of start/period is milli-seconds.
 */
int
timer_periodic(thread_t th, u_long start, u_long period)
{
	struct timer *tmr;
	int err = 0;

	ASSERT(irq_level == 0);

	if (start != 0 && period == 0)
		return EINVAL;

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task != cur_task()) {
		sched_unlock();
		return EPERM;
	}
	tmr = th->periodic;
	if (start == 0) {
		if (tmr != NULL && tmr->active)
			timer_stop(tmr);
		else
			err = EINVAL;
	} else {
		if (tmr == NULL) {
			/*
			 * Allocate a timer element at first call.
			 * We don't put this data in the thread
			 * structure because only a few threads
			 * will use the periodic timer function.
			 */
			tmr = kmem_alloc(sizeof(tmr));
			if (tmr == NULL) {
				sched_unlock();
				return ENOMEM;
			}
			event_init(&tmr->event, "periodic");
			tmr->active = 1;
			th->periodic = tmr;
		}
		/*
		 * Program an interval timer.
		 */
		irq_lock();
		tmr->interval = msec_to_tick(period);
		if (tmr->interval == 0)
			tmr->interval = 1;
		timer_add(tmr, msec_to_tick(start));
		irq_unlock();
	}
	sched_unlock();
	return err;
}


/*
 * timer_waitperiod - wait next period of the periodic timer.
 *
 * Since this routine can exit by any exceptions, the control
 * may return at non-period time. So, the caller must retry
 * immediately if the error status is EINTR. This will be
 * automatically done by the library stub routine.
 */
int
timer_waitperiod(void)
{
	struct timer *tmr;
	int rc, err = 0;

	ASSERT(irq_level == 0);

	if ((tmr = cur_thread->periodic) == NULL)
		return EINVAL;

	if (time_before(lbolt, tmr->expire)) {
		/*
		 * Sleep until timer_tick() routine
		 * wakes us up.
		 */
		rc = sched_sleep(&tmr->event);
		if (rc != SLP_SUCCESS)
			err = EINTR;
	}
	return err;
}

/*
 * Clean up our resource for the thread termination.
 */
void
timer_cleanup(thread_t th)
{

	if (th->periodic != NULL) {
		timer_stop(th->periodic);
		kmem_free(th->periodic);
	}
}

/*
 * Install a timer hook routine.
 * We allow only one hook routine in system.
 */
int
timer_hook(void (*func)(int))
{

	if (tick_hook != NULL)
		return -1;
	irq_lock();
	tick_hook = func;
	irq_unlock();
	return 0;
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
		sched_sleep(&timer_event);

		while (!list_empty(&expire_list)) {
			/*
			 * Callout
			 */
			tmr = list_entry(list_first(&expire_list),
					 struct timer, link);
			list_remove(&tmr->link);
			tmr->active = 0;
			sched_lock();
			interrupt_enable();
			(*tmr->func)(tmr->arg);

			/*
			 * Unlock scheduler here in order to give
			 * chance to higher priority threads to run.
			 */
			sched_unlock();
			interrupt_disable();
		}
	}
	/* NOTREACHED */
}

/*
 * Timer tick handler
 *
 * timer_tick() is called straight from the real time
 * clock interrupt.  All interrupts are still disabled
 * at the entry of this routine.
 */
void
timer_tick(void)
{
	struct timer *tmr;
	u_long ticks;
	int idle, wakeup = 0;

	/*
	 * Bump time in ticks.
	 * Note that it is allowed to wrap.
	 */
	lbolt++;

	/*
	 * Handle all of the timer elements that have expired.
	 */
	while (!list_empty(&timer_list)) {
		/*
		 * Check timer expiration.
		 */
		tmr = timer_next();
		if (time_before(lbolt, tmr->expire))
			break;
		/*
		 * Remove an expired timer from the list and wakup
		 * the appropriate thread. If it is periodic timer,
		 * reprogram the next expiration time. Otherwise,
		 * it is moved to the expired list.
		 */
		list_remove(&tmr->link);
		if (tmr->interval != 0) {
			/*
			 * Periodic timer
			 */
			ticks = time_remain(tmr->expire + tmr->interval);
			if (ticks == 0)
				ticks = 1;
			timer_add(tmr, ticks);
			sched_wakeup(&tmr->event);
		} else {
			/*
			 * One-shot timer
			 */
			list_insert(&expire_list, &tmr->link);
			wakeup = 1;
		}
	}
	if (wakeup)
		sched_wakeup(&timer_event);

	sched_tick();

	/*
	 * Call a hook routine for power management
	 * or profiling work.
	 */
	if (tick_hook != NULL) {
		idle = (cur_thread->prio == PRIO_IDLE) ? 1 : 0;
		(*tick_hook)(idle);
	}
}

u_long
timer_count(void)
{

	return lbolt;
}

void
timer_info(struct info_timer *info)
{

	info->hz = HZ;
}

/*
 * Initialize the timer facility, called at system startup time.
 */
void
timer_init(void)
{

	list_init(&timer_list);
	list_init(&expire_list);
	event_init(&timer_event, "timer");
	event_init(&delay_event, "delay");

	/* Start timer thread */
	if (kthread_create(&timer_thread, NULL, PRIO_TIMER) == NULL)
		panic("timer_init");
}
