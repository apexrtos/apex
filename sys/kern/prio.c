#include <prio.h>

#include <debug.h>
#include <errno.h>
#include <futex.h>
#include <kernel.h>
#include <sch.h>
#include <sig.h>
#include <sync.h>
#include <thread.h>

#define pdbg(...)
//#define pdbg(...) dbg(__VA_ARGS__)

/*
 * prio_inherit
 *
 * Make sure that the thread waiter is waiting on is running at a
 * priority level at least as high as waiter.
 *
 * Return EDEADLK if a deadlock condition is detected.
 */
int
prio_inherit(struct thread *waiter)
{
	struct thread *th = waiter;

	while (th->wait_futex || th->wait_mutex) {
		if (unlikely(th->wait_futex && th->wait_mutex))
			panic("BUG: wait_futex and wait_mutex both set");

		pdbg("t %s m %p f %p ",
		    th->name, th->wait_mutex, th->wait_futex);

		if (th->wait_mutex) {
			if (waiter->prio < mutex_prio(th->wait_mutex))
				mutex_setprio(th->wait_mutex, waiter->prio);
			th = mutex_owner(th->wait_mutex);
		} else if (th->wait_futex) {
			if (waiter->prio < th->wait_futex->prio)
				th->wait_futex->prio = waiter->prio;
			th = th->wait_futex->owner;
		}

		if (th == waiter)
			return DERR(-EDEADLK);

		if (!thread_valid(th)) {
			pdbg("th %p\n", th);
			return DERR(-EFAULT);
		}

		if (th->prio > waiter->prio) {
			pdbg("o %s w %s p %d i %d ws %d\n",
			    th->name, waiter->name, th->prio, waiter->prio,
			    waiter->state);
			sch_setprio(th, th->baseprio, waiter->prio);
		}
	}

	return 0;
}

/*
 * prio_reset
 *
 * Reset specified thread to it's base priority unless it holds a lock or has
 * pending unblocked signal.
 *
 * If the thread holds a lock it's priority is set to that of the
 * highest priority lock.
 */
void
prio_reset(struct thread *th)
{
	/* has this thread been adjusted (by PI or by OP_SETPRIO) */
	if (th->baseprio == th->prio)
		return;

	pdbg("prio_reset th %s b %d p %d\n", th->name, th->baseprio, th->prio);

	struct list *head, *n;
	int top_prio = sig_unblocked_pending(th) ? PRI_SIGNAL : th->baseprio;

	/* search mutexes */
	head = &th->mutexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		struct mutex *m = mutex_entry(n);
		if (mutex_prio(m) < top_prio)
			top_prio = mutex_prio(m);
	}

	/* search futexes */
	head = &th->futexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		struct futex *f = list_entry(n, struct futex, lock_link);
		if (f->prio < top_prio)
			top_prio = f->prio;
	}

	/* set priority */
	sch_setprio(th, th->baseprio, top_prio);
}
