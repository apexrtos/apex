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
 * The APEX scheduler is based on the algorithm known as priority
 * based multi level queue. Each thread has its own priority
 * assigned between 0 and 255. The lower number means higher
 * priority like BSD UNIX.  The scheduler maintains 256 level run
 * queues mapped to each priority.  The lowest priority (=255) is
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
 */

#include <sch.h>

#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <fs.h>
#include <irq.h>
#include <kernel.h>
#include <sched.h>
#include <sections.h>
#include <sig.h>
#include <stddef.h>
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
static struct queue	wakeq;		/* queue for waking threads */
static struct queue	dpcq;		/* DPC queue */
static struct event	dpc_event;	/* event for DPC */

/* currently active thread */
extern struct thread idle_thread;
__attribute__((used)) __fast_data struct thread *active_thread = &idle_thread;
__fast_data static struct list zombie_list = LIST_INIT(zombie_list);

/*
 * Return priority of highest-priority runnable thread.
 */
static int
runq_top(void)
{
	if (queue_empty(&runq))
		return PRI_MIN;

	struct thread *th = queue_entry(queue_first(&runq), struct thread, link);
	return th->prio;
}

/*
 * Insert a thread into the run queue after all threads of higher or equal priority.
 */
static void
runq_enqueue(struct thread *th)
{
	struct queue *q = queue_first(&runq);
	while (!queue_end(&runq, q)) {
		struct thread *qth = queue_entry(q, struct thread, link);
		if (th->prio < qth->prio)
			break;
		q = queue_next(q);
	}

	queue_insert(queue_prev(q), &th->link);

	/* it is only preemption when resched is not pending */
	if (th->prio < active_thread->prio && active_thread->resched == 0)
		active_thread->resched = RESCHED_PREEMPT;
}

/*
 * Insert a thread into the run queue after all threads of higher priority but before
 * threads of equal priority.
 */
static void
runq_insert(struct thread *th)
{
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
	queue_remove(&th->link);
}

/*
 * return true if thread is in runnable state
 */
static bool
thread_runnable(const struct thread *th)
{
	return !(th->state & (TH_SLEEP | TH_SUSPEND | TH_ZOMBIE));
}

/*
 * Process all pending woken threads.
 * Please refer to the comment of sch_wakeup().
 */
static void
wakeq_flush(void)
{
	struct queue *q;
	struct thread *th;

	while (!queue_empty(&wakeq)) {
		/*
		 * Set a thread runnable.
		 */
		q = dequeue(&wakeq);
		th = queue_entry(q, struct thread, link);
		th->slpevt = NULL;
		th->state &= ~TH_SLEEP;
		assert(th != active_thread);
		assert(thread_runnable(th));
		runq_enqueue(th);
	}
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
static void
sch_switch(void)
{
	struct thread *prev, *next;

	/*
	 * Move a current thread to the run queue.
	 */
	prev = active_thread;
	if (thread_runnable(prev)) {
		if (prev->resched == RESCHED_PREEMPT)
			runq_insert(prev);
		else
			runq_enqueue(prev);
	}
	prev->resched = 0;

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
#endif
		list_remove(&prev->task_link);
		list_insert(&zombie_list, &prev->task_link);
	}

	/*
	 * Switch to the new thread.
	 * You are expected to understand this..
	 */
	context_switch(prev, next);
}

/*
 * sleep_expire - sleep timer is expired:
 *
 * Wake up the thread which is sleeping in sched_tsleep().
 */
static void
sleep_expire(void *arg)
{
	sch_unsleep(arg, SLP_TIMEOUT);
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
 * sch_sleep - sleep the current thread until a wakeup
 * is performed on the specified event.
 */
int
sch_sleep(struct event *evt)
{
	return sch_nanosleep(evt, 0);
}

/*
 * sch_nanosleep - sleep the current thread until a wakeup
 * is performed on the specified event.
 *
 * This routine returns a sleep result. If the thread is
 * woken by sch_wakeup() or sch_wakeone(), it returns 0.
 * Otherwise, it will return the result value which is passed
 * by sch_unsleep().  We allow calling sch_sleep() with
 * interrupt disabled.
 *
 * sch_sleep() is also defined as a wrapper for
 * sch_nanosleep() without timeout. Note that all sleep
 * requests are interruptible with this kernel.
 */
int
sch_nanosleep(struct event *evt, uint64_t nsec)
{
	int ret;

	assert(evt);

	sch_lock();

	if (sig_unblocked_pending(active_thread))
		active_thread->slpret = SLP_INTR;

	if (active_thread->slpret != 0)
		goto out;

	const int s = irq_disable();
	active_thread->slpevt = evt;
	active_thread->state |= TH_SLEEP;
	enqueue(&evt->sleepq, &active_thread->link);

	if (nsec != 0) {
		/*
		 * Program timer to wake us up at timeout.
		 */
		timer_callout(&active_thread->timeout, nsec, 0, &sleep_expire,
			      active_thread);
	}
	wakeq_flush();
	sch_switch();	/* Sleep here. Zzzz.. */
	irq_restore(s);

out:
	ret = active_thread->slpret;
	active_thread->slpret = 0;
	sch_unlock();
	return ret;
}

/*
 * sch_wakeup - wake up all threads sleeping on event.
 *
 * A thread can have sleep and suspend state simultaneously.
 * So, the thread may keep suspending even if it woke up.
 *
 * Since this routine can be called from ISR at interrupt
 * level, accessing runq here will require irq_lock() in all
 * other runq accesses. Thus, this routine will temporary move
 * the target thread into wakeq, and they will be moved to runq
 * at non-interrupt level in wakeq_flush().
 *
 * The woken thread will be put on the tail of runq
 * regardless of its scheduling policy. If woken threads have
 * same priority, next running thread is selected in FIFO order.
 *
 * Returns number of threads woken up.
 */
unsigned
sch_wakeup(struct event *evt, int result)
{
	int n;
	struct queue *q;
	struct thread *th;

	assert(evt);

	sch_lock();
	const int s = irq_disable();
	while (!queue_empty(&evt->sleepq)) {
		/*
		 * Move a sleeping thread to the wake queue.
		 */
		q = dequeue(&evt->sleepq);
		th = queue_entry(q, struct thread, link);
		th->slpret = result;
		enqueue(&wakeq, q);
		timer_stop(&th->timeout);
		++n;
	}
	irq_restore(s);
	sch_unlock();

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

	sch_lock();
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
		enqueue(&wakeq, &top->link);
		timer_stop(&top->timeout);
	}
	irq_restore(s);
	sch_unlock();
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

	sch_lock();
	const int s = irq_disable();
	if (!queue_empty(&l->sleepq)) {
		q = dequeue(&l->sleepq);
		th = queue_entry(q, struct thread, link);
		enqueue(&r->sleepq, q);
		timer_redirect(&th->timeout, &sleep_expire, th);
	}
	irq_restore(s);
	sch_unlock();

	return th;
}

/*
 * sch_unsleep - cancel sleep.
 *
 * sch_unsleep() removes the specified thread from its
 * sleep queue. The specified sleep result will be passed
 * to the sleeping thread as a return value of sch_tsleep().
 */
void
sch_unsleep(struct thread *th, int result)
{
	sch_lock();
	if (th->state & TH_SLEEP) {
		const int s = irq_disable();
		queue_remove(&th->link);
		th->slpret = result;
		enqueue(&wakeq, &th->link);
		timer_stop(&th->timeout);
		irq_restore(s);
	}
	sch_unlock();
}

/*
 * sch_interrupt - interrupt a thread.
 *
 * sch_interrupt() guarantees that the thread will return from one sleep
 * as soon as possible.
 */
void
sch_interrupt(struct thread *th)
{
	sch_lock();
	if (th->state & TH_SLEEP)
		sch_unsleep(th, SLP_INTR);
	sch_unlock();
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
	sch_lock();

	if (runq_top() <= active_thread->prio)
		active_thread->resched = RESCHED_SWITCH;

	sch_unlock();	/* Switch current thread here */
}

/*
 * Suspend the specified thread.
 * Called with scheduler locked.
 */
void
sch_suspend(struct thread *th)
{
	assert(sch_locked());
	assert(!(th->state & TH_SUSPEND));

	if (thread_runnable(th)) {
		if (th == active_thread)
			th->resched = RESCHED_SWITCH;
		else
			runq_remove(th);
	}
	th->state |= TH_SUSPEND;
}

/*
 * Resume the specified thread.
 * Called with scheduler locked.
 */
void
sch_resume(struct thread *th)
{
	assert(sch_locked());
	assert(th->state & TH_SUSPEND);

	th->state &= ~TH_SUSPEND;
	if (thread_runnable(th))
		runq_enqueue(th);
}

/*
 * sch_elapse() is called from timer_tick() when time advances.
 * Check quantum expiration, and mark a rescheduling flag.
 * We don't need locking in here.
 */
__fast_text void
sch_elapse(uint32_t nsec)
{
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
			active_thread->resched = RESCHED_SWITCH;
		}
	}
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
	th->state |= TH_EXIT;
}

/*
 * Thread is ready to quit
 */
bool
sch_exit(void)
{
	if (!(active_thread->state & TH_EXIT))
		return false;

	/* cleanup filesystem if this is the last thread in the task */
	if (list_only_entry(&active_thread->task_link))
		fs_exit(active_thread->task);

	sch_lock();
	active_thread->state |= TH_ZOMBIE;
	active_thread->resched = RESCHED_SWITCH;
	sch_unlock();

	return true;
}

/*
 * sch_lock - lock the scheduler.
 *
 * The thread switch is disabled during scheduler locked.
 * This is mainly used to synchronize the thread execution
 * to protect global resources. Even when scheduler is
 * locked, an interrupt handler can run. So, we have to
 * use irq_lock() to synchronize a global data with ISR.
 *
 * Since the scheduling lock can be nested any number of
 * times, the caller has the responsible to unlock the same
 * number of locks.
 */
inline void
sch_lock(void)
{
	active_thread->locks++;
	thread_check();
}

/*
 * sch_unlock - unlock scheduler.
 *
 * If nobody locks the scheduler anymore, it checks the
 * rescheduling flag and kick the scheduler if it's required.
 *
 * Note that this routine will be always called at the end
 * of each interrupt handler.
 */
static void
sch_unlock_slowpath(int s)
{
	wakeq_flush();
	while (active_thread->resched) {

		/* Kick scheduler */
		sch_switch();

		/*
		 * Now we run pending interrupts which fired
		 * during the thread switch. So, we can catch
		 * the rescheduling request from such ISRs.
		 * Otherwise, the reschedule may be deferred
		 * until _next_ sch_unlock() call.
		 */
		irq_restore(s);

		/*
		 * Reap zombies
		 */
		while (!list_empty(&zombie_list)) {
			struct thread *th = list_entry(list_first(&zombie_list),
			    struct thread, task_link);
			list_remove(&th->task_link);
			thread_free(th);
		}

		s = irq_disable();
		wakeq_flush();
	}
}

inline void
sch_unlock(void)
{
	assert(active_thread->locks > 0);
	thread_check();

	int s = irq_disable();
	if (active_thread->locks == 1 &&
	    (active_thread->resched || !queue_empty(&wakeq)))
		sch_unlock_slowpath(s);
	--active_thread->locks;
	irq_restore(s);
}

/*
 * sch_locked - check if scheduler is locked.
 */
bool
sch_locked(void)
{
	return active_thread->locks > 0;
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
 * Called with scheduler locked.
 */
void
sch_setprio(struct thread *th, int baseprio, int prio)
{
	assert(sch_locked());

	th->baseprio = baseprio;

	if (th == active_thread) {
		/*
		 * If we change the current thread's priority
		 * it may be preempted.
		 */
		th->prio = prio;
		/* it is only preemption when resched is not pending */
		if (prio > runq_top() && active_thread->resched == 0)
			active_thread->resched = RESCHED_PREEMPT;
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
	sch_wakeup(&dpc_event, SLP_SUCCESS);

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
		sch_sleep(&dpc_event);
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
	queue_init(&wakeq);
	queue_init(&dpcq);
	event_init(&dpc_event, "dpc", ev_SLEEP);
	active_thread->resched = RESCHED_SWITCH;

	/* Create a DPC thread. */
	th = kthread_create(dpc_thread, NULL, PRI_DPC, "dpc", MEM_FAST);
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
