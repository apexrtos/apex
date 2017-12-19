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
 * mutex.c - mutual exclusion service.
 */

/*
 * A mutex is used to protect un-sharable resources. A thread
 * can use mutex_lock() to ensure that global resource is not
 * accessed by other thread. The mutex is effective only the
 * threads belonging to the same task.
 *
 * Prex will change the thread priority to prevent priority inversion.
 *
 * <Priority inheritance>
 *   The priority is changed at the following conditions.
 *
 *   1. When the current thread can not lock the mutex and its
 *      mutex owner has lower priority than current thread, the
 *      priority of mutex owner is boosted to same priority with
 *      current thread.  If this mutex owner is waiting for another
 *      mutex, such related mutexes are also processed.
 *
 *   2. When the current thread unlocks the mutex and its priority
 *      has already been inherited, the current priority is reset.
 *      In this time, the current priority is changed to the highest
 *      priority among the threads waiting for the mutexes locked by
 *      current thread.
 *
 *   3. When the thread priority is changed by user request, the
 *      inherited thread's priority is changed.
 *
 * <Limitation>
 *
 *   1. If the priority is changed by user request, the priority
 *      recomputation is done only when the new priority is higher
 *      than old priority. The inherited priority is reset to base
 *      priority when the mutex is unlocked.
 *
 *   2. Even if thread is killed with mutex waiting, the related
 *      priority is not adjusted.
 */

#include <kernel.h>
#include <event.h>
#include <sched.h>
#include <kmem.h>
#include <thread.h>
#include <task.h>
#include <sync.h>

/* max mutex count to inherit priority */
#define MAXINHERIT	10

/* forward declarations */
static int	prio_inherit(thread_t th);
static void	prio_uninherit(thread_t th);

/*
 * Initialize a mutex.
 *
 * If an initialized mutex is reinitialized, undefined
 * behavior results. Technically, we can not detect such
 * error condition here because we can not touch the passed
 * object in kernel.
 */
int
mutex_init(mutex_t *mtx)
{
	mutex_t m;

	if ((m = kmem_alloc(sizeof(struct mutex))) == NULL)
		return ENOMEM;

	event_init(&m->event, "mutex");
	m->task = cur_task();
	m->owner = NULL;
	m->prio = MIN_PRIO;
	m->magic = MUTEX_MAGIC;

	if (umem_copyout(&m, mtx, sizeof(m))) {
		kmem_free(m);
		return EFAULT;
	}
	return 0;
}

/*
 * Destroy the specified mutex.
 * The mutex must be unlock state, otherwise it fails with EBUSY.
 */
int
mutex_destroy(mutex_t *mtx)
{
	mutex_t m;
	int err = 0;

	sched_lock();
	if (umem_copyin(mtx, &m, sizeof(mtx))) {
		err = EFAULT;
		goto out;
	}
	if (!mutex_valid(m)) {
		err = EINVAL;
		goto out;
	}
	if (m->owner || event_waiting(&m->event)) {
		err = EBUSY;
		goto out;
	}

	m->magic = 0;
	kmem_free(m);
 out:
	sched_unlock();
	ASSERT(err == 0);
	return err;
}

/*
 * Copy mutex from user space.
 * If it is not initialized, create new mutex.
 */
static int
mutex_copyin(mutex_t *umtx, mutex_t *kmtx)
{
	mutex_t m;
	int err;

	if (umem_copyin(umtx, &m, sizeof(umtx)))
		return EFAULT;

	if (m == MUTEX_INITIALIZER) {
		/*
		 * Allocate new mutex, and retreive its id
		 * from the user space.
		 */
		if ((err = mutex_init(umtx)))
			return err;
		umem_copyin(umtx, &m, sizeof(umtx));
	} else {
		if (!mutex_valid(m))
			return EINVAL;
	}
	*kmtx = m;
	return 0;
}

/*
 * Lock a mutex.
 *
 * A current thread is blocked if the mutex has already been
 * locked. If current thread receives any exception while
 * waiting mutex, this routine returns with EINTR in order to
 * invoke exception handler. But, POSIX thread assumes this
 * function does NOT return with EINTR.  So, system call stub
 * routine in library must call this again if it gets EINTR.
 */
int
mutex_lock(mutex_t *mtx)
{
	mutex_t m;
	int rc, err;

	sched_lock();
	if ((err = mutex_copyin(mtx, &m)))
		goto out;

	if (m->owner == cur_thread) {
		/*
		 * Recursive lock
		 */
		m->locks++;
		ASSERT(m->locks != 0);
	} else {
		/*
		 * Check whether a target mutex is locked.
		 * If the mutex is not locked, this routine
		 * returns immediately.
		 */
		if (m->owner == NULL)
			m->prio = cur_thread->prio;
		else {
			/*
			 * Wait for a mutex.
			 */
			cur_thread->wait_mutex = m;
			if ((err = prio_inherit(cur_thread))) {
				cur_thread->wait_mutex = NULL;
				goto out;
			}
			rc = sched_sleep(&m->event);
			cur_thread->wait_mutex = NULL;
			if (rc == SLP_INTR) {
				err = EINTR;
				goto out;
			}
		}
		m->locks = 1;
	}
	m->owner = cur_thread;
	list_insert(&cur_thread->mutexes, &m->link);
 out:
	sched_unlock();
	return err;
}

/*
 * Try to lock a mutex without blocking.
 */
int
mutex_trylock(mutex_t *mtx)
{
	mutex_t m;
	int err;

	sched_lock();
	if ((err = mutex_copyin(mtx, &m)))
		goto out;
	if (m->owner == cur_thread)
		m->locks++;
	else {
		if (m->owner != NULL)
			err = EBUSY;
		else {
			m->locks = 1;
			m->owner = cur_thread;
			list_insert(&cur_thread->mutexes, &m->link);
		}
	}
 out:
	sched_unlock();
	return err;
}

/*
 * Unlock a mutex.
 * Caller must be a current mutex owner.
 */
int
mutex_unlock(mutex_t *mtx)
{
	mutex_t m;
	int err;

	sched_lock();
	if ((err = mutex_copyin(mtx, &m)))
		goto out;
	if (m->owner != cur_thread || m->locks <= 0) {
		err = EPERM;
		goto out;
	}
	if (--m->locks == 0) {
		list_remove(&m->link);
		prio_uninherit(cur_thread);
		/*
		 * Change the mutex owner, and make the next
		 * owner runnable if it exists.
		 */
		m->owner = sched_wakeone(&m->event);
		if (m->owner)
			m->owner->wait_mutex = NULL;

		m->prio = m->owner ? m->owner->prio : MIN_PRIO;
	}
 out:
	sched_unlock();
	ASSERT(err == 0);
	return err;
}

/*
 * Clean up mutex.
 *
 * This is called with scheduling locked when thread is
 * terminated. If a thread is terminated with mutex hold, all
 * waiting threads keeps waiting forever. So, all mutex locked by
 * terminated thread must be unlocked. Even if the terminated
 * thread is waiting some mutex, the inherited priority of other
 * mutex owner is not adjusted.
 */
void
mutex_cleanup(thread_t th)
{
	list_t head;
	mutex_t m;
	thread_t owner;

	/*
	 * Purge all mutexes held by the thread.
	 */
	head = &th->mutexes;
	while (!list_empty(head)) {
		/*
		 * Release locked mutex.
		 */
		m = list_entry(list_first(head), struct mutex, link);
		m->locks = 0;
		list_remove(&m->link);
		/*
		 * Change the mutex owner if other thread
		 * is waiting for it.
		 */
		owner = sched_wakeone(&m->event);
		if (owner) {
			owner->wait_mutex = NULL;
			m->locks = 1;
			list_insert(&owner->mutexes, &m->link);
		}
		m->owner = owner;
	}
}

/*
 * This is called with scheduling locked before thread priority
 * is changed.
 */
void
mutex_setprio(thread_t th, int prio)
{
	if (th->wait_mutex && prio < th->prio)
		prio_inherit(th);
}

/*
 * Inherit priority.
 *
 * To prevent priority inversion, we must ensure the higher
 * priority thread does not wait other lower priority thread. So,
 * raise the priority of mutex owner which blocks the "waiter"
 * thread. If such mutex owner is also waiting for other mutex,
 * that mutex is also processed. Returns EDEALK if it finds
 * deadlock condition.
 */
static int
prio_inherit(thread_t waiter)
{
	mutex_t m = waiter->wait_mutex;
	thread_t owner;
	int count = 0;

	do {
		owner = m->owner;
		/*
		 * If the owner of relative mutex has already
		 * been waiting for the "waiter" thread, it
		 * causes a deadlock.
		 */
		if (owner == waiter) {
			DPRINTF(("Deadlock! mutex=%x owner=%x waiter=%x\n",
				 m, owner, waiter));
			return EDEADLK;
		}
		/*
		 * If the priority of the mutex owner is lower
		 * than "waiter" thread's, we rise the mutex
		 * owner's priority.
		 */
		if (owner->prio > waiter->prio) {
			sched_setprio(owner, owner->baseprio, waiter->prio);
			m->prio = waiter->prio;
		}
		/*
		 * If the mutex owner is waiting for another
		 * mutex, that mutex is also processed.
		 */
		m = (mutex_t)owner->wait_mutex;

		/* Fail safe... */
		ASSERT(count < MAXINHERIT);
		if (count >= MAXINHERIT)
			break;

	} while (m != NULL);
	return 0;
}

/*
 * Un-inherit priority
 *
 * The priority of specified thread is reset to the base
 * priority.  If specified thread locks other mutex and higher
 * priority thread is waiting for it, the priority is kept to
 * that level.
 */
static void
prio_uninherit(thread_t th)
{
	int top_prio;
	list_t head, n;
	mutex_t m;

	/* Check if the priority is inherited. */
	if (th->prio == th->baseprio)
		return;

	top_prio = th->baseprio;
	/*
	 * Find the highest priority thread that is waiting
	 * for the thread. This is done by checking all mutexes
	 * that the thread locks.
	 */
	head = &th->mutexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		m = list_entry(n, struct mutex, link);
		if (m->prio < top_prio)
			top_prio = m->prio;
	}
	sched_setprio(th, th->baseprio, top_prio);
}
