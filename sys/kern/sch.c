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

/*
 * sched.c - scheduler
 */

/**
 * General design:
 *
 * The APEX scheduler is based on the algorithm known as priority based queue.
 * Each thread has its own priority assigned between 0 and 255. The lower
 * number means higher priority like BSD UNIX. The lowest priority (=255) is
 * used only for an idle thread.
 *
 * All threads have two different types of priorities:
 *
 *  Base priority:
 *      This is a static priority used for priority computation.
 *      A user mode program can change this value via system call.
 *
 *  Current priority:
 *      An actual scheduling priority. A kernel may adjust this
 *      priority dynamically if it's needed.
 *
 * Each thread has one of the following state.
 *
 *  - TH_SLEEP   Sleep for some event
 *  - TH_SUSPEND Suspend count is not 0
 *  - TH_EXIT    Terminated
 *  - TH_ZOMBIE  Ready to be freed
 *
 * The thread is always preemptive even in the kernel mode.
 * There are following 4 reasons to switch thread.
 *
 * (1) Block
 *      Thread is blocked for sleep or suspend.
 *      It is put on the tail of the run queue when it becomes
 *      runnable again.
 *
 * (2) Preemption
 *      If higher priority thread becomes runnable, the current
 *      thread is put on the _head_ of the run queue.
 *
 * (3) Quantum expiration
 *      If the thread consumes its time quantum, it is put on
 *      the tail of the run queue.
 *
 * (4) Yield
 *      If the thread releases CPU by itself, it is put on the
 *      tail of the run queue.
 *
 * There are following three types of scheduling policies.
 *
 *  - SCHED_FIFO   First in-first-out
 *  - SCHED_RR     Round robin (SCHED_FIFO + timeslice)
 *  - SCHED_OTHER  Another scheduling (not supported)
 *
 * TODO: look at combining resched & locks into single atomic?
 */

#include <sch.h>

#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <irq.h>
#include <kernel.h>
#include <sched.h>
#include <sections.h>
#include <sig.h>
#include <stddef.h>
#include <task.h>
#include <thread.h>
#include <types.h>

/*
 * Scheduling quantum (nanoseconds for context switch)
 */
#define QUANTUM (CONFIG_TIME_SLICE_MS * 1000000)

#define RESCHED_SWITCH  1
#define RESCHED_PREEMPT 2

#define DPC_FREE	0x4470463f	/* 'DpF?' */
#define DPC_PENDING	0x4470503f	/* 'DpP?' */

static struct queue	runq;		/* run queue */
static struct queue	dpcq;		/* DPC queue */
static struct event	dpc_event;	/* event for DPC */

/* currently active thread */
extern struct thread idle_thread;
__attribute__((used)) __fast_data struct thread *active_thread = &idle_thread;
__fast_bss static int resched;
__fast_bss static int locks;

/*
 * Return priority of highest-priority runnable thread.
 */
static int
runq_top(void)
{
	assert(!interrupt_enabled());

	if (queue_empty(&runq))
		return PRI_MIN + 1;

	struct thread *th = queue_entry(queue_first(&runq), struct thread, link);
	return th->prio;
}

/*
 * return true if thread is in runnable state
 */
static bool
thread_runnable(const struct thread *th)
{
	assert(!interrupt_enabled());

	return !(th->state & (TH_SLEEP | TH_SUSPEND | TH_ZOMBIE));
}

/*
 * Insert a thread into the run queue after all threads of higher or equal priority.
 */
static void
runq_enqueue(struct thread *th)
{
	assert(!interrupt_enabled());
	assert(thread_runnable(th));

	struct queue *q = queue_first(&runq);
	while (!queue_end(&runq, q)) {
		struct thread *qth = queue_entry(q, struct thread, link);
		if (th->prio < qth->prio)
			break;
		q = queue_next(q);
	}

	queue_insert(queue_prev(q), &th->link);

	/* it is only preemption when resched is not pending */
	if (th->prio < active_thread->prio && resched == 0)
		resched = RESCHED_PREEMPT;
}

/*
 * Insert a thread into the run queue after all threads of higher priority but before
 * threads of equal priority.
 */
static void
runq_insert(struct thread *th)
{
	assert(!interrupt_enabled());

	struct queue *q = queue_first(&runq);
	while (!queue_end(&runq, q)) {
		struct thread *qth = queue_entry(q, struct thread, link);
		if (th->prio <= qth->prio)
			break;
		q = queue_next(q);
	}

	queue_insert(queue_prev(q), &th->link);
}

/*
 * Pick up and remove the highest-priority thread
 * from the run queue.
 */
static struct thread *
runq_dequeue(void)
{
	assert(!interrupt_enabled());

	struct thread *th;

	th = queue_entry(queue_first(&runq), struct thread, link);
	queue_remove(&th->link);
	return th;
}

/*
 * Remove the specified thread from the run queue.
 */
static void
runq_remove(struct thread *th)
{
	assert(!interrupt_enabled());

	queue_remove(&th->link);
}

/*
 * Request reschedule if current thread needs to be switched
 */
static void
schedule(void)
{
	assert(!interrupt_enabled());

	if (!locks && resched)
		arch_schedule();
}

/*
 * sleep_expire - sleep timer is expired:
 *
 * Wake up the thread which is sleeping in sched_tsleep().
 */
static void
sleep_expire(void *arg)
{
	sch_unsleep(arg, -ETIMEDOUT);
}

/*
 * sch_switch - this is the scheduler proper:
 *
 * If the scheduling reason is preemption, the current
 * thread will remain at the head of the run queue.  So,
 * the thread still has right to run first again among
 * the same priority threads. For other scheduling reason,
 * the current thread is inserted into the tail of the run
 * queue.
 */
__fast_text void
sch_switch(void)
{
	assert(!interrupt_enabled());

	struct thread *prev, *next;

	/*
	 * Ignore spurious sch_switch calls.
	 */
	if (!resched)
		return;

	/*
	 * Switching threads with preemption disabled makes no sense!
	 */
	assert(!locks);

	/*
	 * Switching threads while holding a spinlock is very bad.
	 */
#if defined(CONFIG_DEBUG)
	assert(!active_thread->spinlock_locks);
#endif

	/*
	 * Move a current thread to the run queue.
	 */
	prev = active_thread;
	if (thread_runnable(prev)) {
		if (resched == RESCHED_PREEMPT)
			runq_insert(prev);
		else
			runq_enqueue(prev);
	}
	resched = 0;

	/*
	 * Select the thread to run the CPU next.
	 */
	next = runq_dequeue();
	if (next == prev)
		return;
	active_thread = next;

	/*
	 * Queue zombie for deletion
	 */
	if (prev->state & TH_ZOMBIE) {
		/*
		 * Reaping a thread holding locks is very bad.
		 */
#if defined(CONFIG_DEBUG)
		assert(!prev->mutex_locks);
		assert(!prev->spinlock_locks);
		assert(!prev->rwlock_locks);
#endif
		sch_wakeup(&prev->task->thread_event, 0);
		list_remove(&prev->task_link);
		thread_zombie(prev);
	}

	/*
	 * Switch to the new thread.
	 * You are expected to understand this..
	 */
	context_switch(prev, next);
}

/*
 * sch_active - get currently active thread
 */
struct thread *
sch_active(void)
{
	return active_thread;
}

/*
 * sch_wakeup - wake up all threads sleeping on event.
 *
 * A thread can have sleep and suspend state simultaneously.
 * So, the thread may keep suspending even if it woke up.
 *
 * Returns number of threads woken up.
 */
unsigned
sch_wakeup(struct event *evt, int result)
{
	int n = 0;
	struct queue *q;
	struct thread *th;

	assert(evt);

	const int s = irq_disable();
	while (!queue_empty(&evt->sleepq)) {
		/*
		 * Move a sleeping thread to the run queue.
		 */
		q = dequeue(&evt->sleepq);
		th = queue_entry(q, struct thread, link);
		th->slpret = result;
		th->slpevt = NULL;
		th->state &= ~TH_SLEEP;
		timer_stop(&th->timeout);
		if (th != active_thread)
			runq_enqueue(th);
		++n;
	}
	if (n)
		schedule();
	irq_restore(s);

	return n;
}

/*
 * sch_wakeone - wake up one thread sleeping on event.
 *
 * The highest priority thread is woken among sleeping
 * threads. This routine returns the thread ID of the
 * woken thread, or NULL if no threads are sleeping.
 */
struct thread *
sch_wakeone(struct event *evt)
{
	struct queue *head, *q;
	struct thread *top = NULL, *th;

	const int s = irq_disable();
	head = &evt->sleepq;
	if (!queue_empty(head)) {
		/*
		 * Select the highet priority thread in
		 * the sleep queue, and wakeup it.
		 */
		q = queue_first(head);
		top = queue_entry(q, struct thread, link);
		while (!queue_end(head, q)) {
			th = queue_entry(q, struct thread, link);
			if (th->prio < top->prio)
				top = th;
			q = queue_next(q);
		}
		queue_remove(&top->link);
		top->slpret = 0;
		top->slpevt = NULL;
		top->state &= ~TH_SLEEP;
		timer_stop(&top->timeout);
		if (th != active_thread)
			runq_enqueue(top);
	}
	if (top)
		schedule();
	irq_restore(s);

	return top;
}

/*
 * sch_requeue - move one thread sleeping on event l to sleeping on event r
 */
struct thread *
sch_requeue(struct event *l, struct event *r)
{
	struct queue *q;
	struct thread *th = 0;

	const int s = irq_disable();
	if (!queue_empty(&l->sleepq)) {
		q = dequeue(&l->sleepq);
		th = queue_entry(q, struct thread, link);
		enqueue(&r->sleepq, q);
		timer_redirect(&th->timeout, &sleep_expire, th);
	}
	irq_restore(s);

	return th;
}

/*
 * sch_prepare_sleep - prepare to sleep on an event
 *
 * If nsec == 0 sch_continue_sleep will sleep without timeout.
 *
 * On success, must be followed by sch_continue_sleep or sch_cancel_sleep.
 */
int
sch_prepare_sleep(struct event *evt, uint_fast64_t nsec)
{
	assert(!(active_thread->state & TH_SLEEP));
	assert(!interrupt_running());
	assert(evt);

	const int s = irq_disable();

	if (sig_unblocked_pending(active_thread)) {
		irq_restore(s);
		return -EINTR;
	}

	active_thread->slpevt = evt;
	active_thread->state |= TH_SLEEP;
	enqueue(&evt->sleepq, &active_thread->link);

	/* program timer to wake us up after nsec */
	if (nsec != 0)
		timer_callout(&active_thread->timeout, nsec, 0,
		    &sleep_expire, active_thread);

	/* disable preemption */
	sch_lock();

	irq_restore(s);

	return 0;
}

/*
 * sch_continue_sleep - sleep on prepared event
 *
 * Must be called after successful sch_prepare_sleep.
 *
 * This routine returns a sleep result. If the thread is woken by sch_wakeone()
 * it returns 0. Otherwise, it will return the result value which is passed to
 * sch_unsleep() or sch_wakeup().
 */
int
sch_continue_sleep()
{
	assert(interrupt_enabled());
	assert(locks == 1);

	interrupt_disable();

	/* enable preemption atomically with interrupts disabled */
	locks = 0;

	/* if we are still going to sleep, sleep now! */
	if (active_thread->state & TH_SLEEP)
		resched = RESCHED_SWITCH;
	if (resched)
		arch_schedule();

	interrupt_enable();

	/* if this assertion fires the CPU port is broken */
	assert(!(active_thread->state & TH_SLEEP));

	return active_thread->slpret;
}

/*
 * sch_cancel_sleep - cancel prepared sleep
 */
void
sch_cancel_sleep(void)
{
	sch_unsleep(active_thread, 0);
	sch_unlock();
}

/*
 * sch_unsleep - cancel sleep.
 *
 * sch_unsleep() removes the specified thread from its
 * sleep queue. The specified sleep result will be passed
 * to the sleeping thread as a return value of sch_tsleep().
 *
 * Callable from interrupt.
 */
void
sch_unsleep(struct thread *th, int result)
{
	const int s = irq_disable();
	if (th->state & TH_SLEEP) {
		queue_remove(&th->link);
		th->slpret = result;
		th->slpevt = NULL;
		th->state &= ~TH_SLEEP;
		timer_stop(&th->timeout);
		if (th != active_thread) {
			runq_enqueue(th);
			schedule();
		}
	}
	irq_restore(s);
}

/*
 * sch_signal - interrupt a thread to deliver signal.
 *
 * Callable from interrupt.
 */
void
sch_signal(struct thread *th)
{
	if (th == active_thread) {
		/* signal will be delivered on return to userspace */
		const int s = irq_disable();
		resched = RESCHED_PREEMPT;
		schedule();
		irq_restore(s);
	} else
		sch_unsleep(th, -EINTR);
}

/*
 * Yield the current processor to another thread.
 *
 * Note that the current thread may run immediately again,
 * if no other thread exists in the same priority queue.
 */
void
sch_yield(void)
{
	assert(!locks);

	const int s = irq_disable();

	if (runq_top() <= active_thread->prio) {
		resched = RESCHED_SWITCH;
		arch_schedule();
	}

	irq_restore(s);
}

/*
 * Suspend the specified thread.
 */
void
sch_suspend(struct thread *th)
{
	sch_suspend_resume(th, NULL);
}

/*
 * Resume the specified thread.
 */
void
sch_resume(struct thread *th)
{
	sch_suspend_resume(NULL, th);
}

/*
 * Atomically suspend one thread and resume another.
 */
void
sch_suspend_resume(struct thread *suspend, struct thread *resume)
{
	bool reschedule = false;

	const int s = irq_disable();

	if (suspend) {
		assert(!(suspend->state & TH_SUSPEND));

		if (suspend == active_thread)
			resched = RESCHED_SWITCH;
		else if (thread_runnable(suspend))
			runq_remove(suspend);
		suspend->state |= TH_SUSPEND;
		reschedule = true;
	}

	if (resume) {
		assert(resume->state & TH_SUSPEND);

		resume->state &= ~TH_SUSPEND;
		if (thread_runnable(resume) && resume != active_thread) {
			runq_enqueue(resume);
			reschedule = true;
		}
	}

	if (reschedule)
		schedule();

	irq_restore(s);
}

/*
 * sch_elapse() is called from timer_tick() when time advances.
 * Check quantum expiration, and mark a rescheduling flag.
 */
__fast_text void
sch_elapse(uint_fast32_t nsec)
{
	const int s = irq_disable();

	/* Profile running time. */
	active_thread->time += nsec;

	if (active_thread->policy == SCHED_RR) {
		active_thread->timeleft -= nsec;
		if (active_thread->timeleft <= 0) {
			/*
			 * The quantum is up.
			 * Give the thread another.
			 */
			active_thread->timeleft += QUANTUM;

			/*
			 * If there are other threads of equal or higher
			 * priority run them now!
			 */
			if (runq_top() <= active_thread->prio) {
				resched = RESCHED_SWITCH;
				schedule();
			}
		}
	}
	irq_restore(s);
}

/*
 * Set up stuff for thread scheduling.
 */
void
sch_start(struct thread *th)
{
	th->state = TH_SUSPEND;
	th->policy = SCHED_RR;
	th->prio = PRI_DEFAULT;
	th->baseprio = PRI_DEFAULT;
	th->timeleft = QUANTUM;
}

/*
 * Tell thread to exit
 */
void
sch_stop(struct thread *th)
{
	const int s = irq_disable();

	th->state |= TH_EXIT;

	irq_restore(s);
}

/*
 * Thread is ready to quit
 */
bool
sch_testexit(void)
{
	assert(interrupt_enabled());
	assert(!locks);

	interrupt_disable();
	if (!(active_thread->state & TH_EXIT)) {
		interrupt_enable();
		return false;
	}

	/* mark thread as zombie */
	active_thread->state |= TH_ZOMBIE;
	resched = RESCHED_SWITCH;
	arch_schedule();
	interrupt_enable();

	return true;
}

/*
 * sch_lock - lock the scheduler.
 *
 * Preemption is disabled while the scheduler is locked.
 *
 * Interrupts still run while preemption is disabled.
 */
inline void
sch_lock(void)
{
	write_once(&locks, locks + 1);
	compiler_barrier();
	thread_check();
}

/*
 * sch_unlock - unlock scheduler.
 *
 * If nobody locks the scheduler anymore, it checks the
 * rescheduling flag and kick the scheduler if it's required.
 */
inline void
sch_unlock(void)
{
	assert(locks > 0);
	assert(locks > 1 || interrupt_enabled());

	thread_check();
	compiler_barrier();
	write_once(&locks, locks - 1);

	if (locks)
		return;

	interrupt_disable();
	if (resched)
		arch_schedule();
	interrupt_enable();
}

/*
 * sch_locks - return number of scheduler locks.
 */
int
sch_locks(void)
{
	return locks;
}

/*
 * sch_getprio - get priority of thread
 */
int
sch_getprio(struct thread *th)
{
	return th->prio;
}

/*
 * sch_setprio - set priority of thread.
 *
 * The rescheduling flag is set if the priority is
 * higher (less than) than the currently running thread.
 */
void
sch_setprio(struct thread *th, int baseprio, int prio)
{
	int s = irq_disable();
	th->baseprio = baseprio;
	if (th == active_thread) {
		/*
		 * If we change the current thread's priority
		 * it may be preempted.
		 */
		th->prio = prio;
		/* it is only preemption when resched is not pending */
		if (prio > runq_top() && resched == 0)
			resched = RESCHED_PREEMPT;
	} else {
		if (thread_runnable(th)) {
			/*
			 * Update the thread priority and adjust
			 * the run queue position for new priority.
			 */
			runq_remove(th);
			th->prio = prio;
			runq_enqueue(th);
		} else
			th->prio = prio;
	}
	schedule();
	irq_restore(s);
}

int
sch_getpolicy(struct thread *th)
{
	return th->policy;
}

int
sch_setpolicy(struct thread *th, int policy)
{
	int err = 0;

	switch (policy) {
	case SCHED_RR:
	case SCHED_FIFO:
		th->timeleft = QUANTUM;
		th->policy = policy;
		break;
	default:
		err = -1;
		break;
	}
	return err;
}

/*
 * Schedule DPC callback.
 *
 * DPC (Deferred Procedure Call) is used to call the specific
 * function at some later time with a DPC priority. It is also
 * known as AST or SoftIRQ in other kernels.  DPC is typically
 * used by device drivers to do the low-priority jobs without
 * degrading real-time performance.
 * This routine can be called from ISR.
 */
void
sch_dpc(struct dpc *dpc, void (*func)(void *), void *arg)
{
	assert(dpc);
	assert(func);

	const int s = irq_disable();
	/*
	 * Insert request to DPC queue.
	 */
	dpc->func = func;
	dpc->arg = arg;
	if (dpc->state != DPC_PENDING)
		enqueue(&dpcq, &dpc->link);
	dpc->state = DPC_PENDING;

	/* Wake DPC thread */
	sch_wakeup(&dpc_event, 0);

	irq_restore(s);
}

/*
 * DPC thread.
 *
 * This is a kernel thread to process the pending call back
 * request within DPC queue. Each DPC routine is called with
 * the following conditions.
 *  - Interrupt is enabled.
 *  - Scheduler is unlocked.
 */
static void
dpc_thread(void *unused_arg)
{
	struct queue *q;
	struct dpc *dpc;

	for (;;) {
		interrupt_disable();
		while (!queue_empty(&dpcq)) {
			q = dequeue(&dpcq);
			dpc = queue_entry(q, struct dpc, link);
			dpc->state = DPC_FREE;
			/* cache data before interrupt_enable()  */
			void (*func)(void *) = dpc->func;
			void *arg = dpc->arg;

			/*
			 * Call DPC routine.
			 */
			interrupt_enable();
			func(arg);
			interrupt_disable();
		}

		/*
		 * Wait until next DPC request. Done after first pass as
		 * there may be some dpc pending from kernel start
		 */
		sch_prepare_sleep(&dpc_event, 0);
		interrupt_enable();
		sch_continue_sleep();
	}
	/* NOTREACHED */
}

void
sch_dump(void)
{
	info("scheduler dump\n");
	info("==============\n");
	info(" thread      th         pri\n");
	info(" ----------- ---------- ---\n");
	struct queue *q = queue_first(&runq);
	while (!queue_end(&runq, q)) {
		struct thread *th = queue_entry(q, struct thread, link);
		info(" %11s %p %3d\n", th->name, th, th->prio);
		q = queue_next(q);
	}
}

/*
 * Initialize the global scheduler state.
 */
void
sch_init(void)
{
	struct thread *th;

	queue_init(&runq);
	queue_init(&dpcq);
	event_init(&dpc_event, "dpc", ev_SLEEP);

	/* Create a DPC thread. */
	th = kthread_create(dpc_thread, NULL, PRI_DPC, "dpc", MA_FAST);
	if (th == NULL)
		panic("sch_init");

	dbg("Time slice is %d msec\n", CONFIG_TIME_SLICE_MS);
}

/*
 * Get maximum scheduling priority for policy
 */
int
sched_get_priority_max(int policy)
{
	if (policy == SCHED_FIFO || policy == SCHED_RR)
		return 100;
	return -EINVAL;
}

/*
 * Get minimum scheduling priority for policy
 */
int
sched_get_priority_min(int policy)
{
	if (policy == SCHED_FIFO || policy == SCHED_RR)
		return 1;
	return -EINVAL;
}
