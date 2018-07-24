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
 * accessed by other thread.
 *
 * APEX will change the thread priority to prevent priority inversion.
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

#include <sync.h>

#include <access.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <prio.h>
#include <sch.h>
#include <sig.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <thread.h>

#define MUTEX_MAGIC    0x4d75783f      /* 'Mux?' */

struct mutex_private {
	int magic;		/* magic number */
	atomic_intptr_t owner;	/* owner thread locking this mutex */
	unsigned count;		/* counter for recursive lock */
	struct event event;	/* event */
	struct list link;	/* linkage on locked mutex list */
	int prio;		/* highest prio in waiting threads */
};

static_assert(sizeof(struct mutex_private) == sizeof(struct mutex), "");
static_assert(alignof(struct mutex_private) == alignof(struct mutex), "");

/*
 * mutex_valid - check mutex validity.
 */
bool
mutex_valid(const struct mutex *m)
{
	const struct mutex_private *mp = (struct mutex_private*)m->storage;

	return k_address(m) && mp->magic == MUTEX_MAGIC;
}

/*
 * mutex_init - Initialize a mutex.
 */
void
mutex_init(struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;

	mp->magic = MUTEX_MAGIC;
	event_init(&mp->event, "mutex", ev_LOCK);
	atomic_store_explicit(&mp->owner, 0, memory_order_relaxed);
	/* No need to initialise link, prio or count */
}

/*
 * mutex_lock - Lock a mutex.
 *
 * The current thread is blocked if the mutex has already been
 * locked. If current thread receives any exception while
 * waiting mutex, this routine returns EINTR.
 */
static int __attribute__((noinline))
mutex_lock_slowpath(struct mutex *m)
{
	if (!mutex_valid(m))
		return DERR(-EINVAL);

	struct mutex_private *mp = (struct mutex_private*)m->storage;

	sch_lock();

	/* check if we already hold the mutex */
	if (mutex_owner(m) == thread_cur()) {
		atomic_fetch_or_explicit(
		    &mp->owner,
		    MUTEX_RECURSIVE,
		    memory_order_relaxed
		);
		++mp->count;
		sch_unlock();
		return 0;
	}

	/* mutex was freed since atomic test */
	intptr_t expected = 0;
	if (atomic_compare_exchange_strong_explicit(
	    &mp->owner,
	    &expected,
	    (intptr_t)thread_cur(),
	    memory_order_acquire,
	    memory_order_relaxed)) {
		mp->count = 1;
		sch_unlock();
		return 0;
	}

	/*
	 * If we are the first waiter we need to add m to the owner's lock list
	 * and initialise the mutex priority.
	 */
	if (!event_waiting(&mp->event)) {
		mp->prio = mutex_owner(m)->prio;
		list_insert(&mutex_owner(m)->mutexes, &mp->link);
	}

	atomic_fetch_or_explicit(
	    &mp->owner,
	    MUTEX_WAITERS,
	    memory_order_relaxed
	);

	/* set wait_mutex and inherit priority */
	thread_cur()->wait_mutex = m;
	prio_inherit(thread_cur());

	/* wait for unlock */
	int err = 0;
	switch (sch_sleep(&mp->event)) {
	case 0:
		/* mutex_unlock will set us as the owner */
		assert(mutex_owner(m) == thread_cur());
		assert(thread_cur()->wait_mutex == NULL);
		break;
	case SLP_INTR:
#if defined(CONFIG_DEBUG)
		--thread_cur()->mutex_locks;
#endif
		thread_cur()->wait_mutex = NULL;
		err = -EINTR;
		break;
	default:
		panic("mutex: bad sleep result");
	}

	sch_unlock();
	return err;
}

int
mutex_lock_interruptible(struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;

#if defined(CONFIG_DEBUG)
	++thread_cur()->mutex_locks;
#endif

	intptr_t expected = 0;
	if (atomic_compare_exchange_strong_explicit(
	    &mp->owner,
	    &expected,
	    (intptr_t)thread_cur(),
	    memory_order_acquire,
	    memory_order_relaxed)) {
		mp->count = 1;
		return 0;
	}

	return mutex_lock_slowpath(m);
}

int
mutex_lock(struct mutex *m)
{
	const k_sigset_t sig_mask = sig_block_all();
	const int ret = mutex_lock_interruptible(m);
	sig_restore(&sig_mask);
	return ret;
}

/*
 * mutex_unlock - Unlock a mutex.
 */
static int __attribute__((noinline))
mutex_unlock_slowpath(struct mutex *m)
{
	if (!mutex_valid(m))
		return DERR(-EINVAL);

	/* can't unlock if we don't hold */
	if (mutex_owner(m) != thread_cur())
		return DERR(-EINVAL);

	struct mutex_private *mp = (struct mutex_private*)m->storage;

	/* check recursive lock */
	if (--mp->count != 0) {
		return 0;
	}

	sch_lock();

	if (!(atomic_load_explicit(
	    &mp->owner,
	    memory_order_relaxed) & MUTEX_WAITERS)) {
		atomic_store_explicit(&mp->owner, 0, memory_order_release);
		sch_unlock();
		return 0;
	}

	/* remove from lock list and reset priority */
	list_remove(&mp->link);
	prio_reset(thread_cur());

	/* wake up one waiter and set new owner */
	atomic_store_explicit(
	    &mp->owner,
	    (intptr_t)sch_wakeone(&mp->event),
	    memory_order_relaxed
	);
	mp->count = 1;
	assert(atomic_load_explicit(&mp->owner, memory_order_relaxed));

	if (event_waiting(&mp->event)) {
		mp->prio = mutex_owner(m)->prio;
		list_insert(&mutex_owner(m)->mutexes, &mp->link);
		atomic_fetch_or_explicit(&mp->owner, MUTEX_WAITERS, memory_order_relaxed);
	}
	mutex_owner(m)->wait_mutex = NULL;

	sch_unlock();
	return 0;
}

int
mutex_unlock(struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;

#if defined(CONFIG_DEBUG)
	--thread_cur()->mutex_locks;
#endif

	intptr_t expected = (intptr_t)thread_cur();
	const intptr_t zero = 0;
	if (atomic_compare_exchange_strong_explicit(
	    &mp->owner,
	    &expected,
	    zero,
	    memory_order_release,
	    memory_order_relaxed))
		return 0;

	return mutex_unlock_slowpath(m);
}

/*
 * mutex_owner - get owner of mutex
 */
struct thread*
mutex_owner(const struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;
	return (struct thread *)(atomic_load_explicit(
	    &mp->owner,
	    memory_order_relaxed) & MUTEX_TID_MASK);
}

/*
 * mutex_prio - get priority of mutex
 */
int
mutex_prio(const struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;
	return mp->prio;
}

/*
 * mutex_setprio - set priority of mutex
 */
void
mutex_setprio(struct mutex *m, int prio)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;
	mp->prio = prio;
}

/*
 * mutex_count - get lock count of mutex
 */
unsigned
mutex_count(const struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;
	return mp->count;
}

/*
 * mutex_entry - get mutex struct from list entry
 */
struct mutex*
mutex_entry(struct list *l)
{
	return (struct mutex*)list_entry(l, struct mutex_private, link);
}
