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
 */

#include <sync.h>

#include <access.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <sch.h>
#include <sig.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <thread.h>

struct mutex_private {
	atomic_intptr_t owner;	/* owner thread locking this mutex */
	unsigned count;		/* counter for recursive lock */
	struct event event;	/* event */
};

static_assert(sizeof(struct mutex_private) == sizeof(struct mutex), "");
static_assert(alignof(struct mutex_private) == alignof(struct mutex), "");

/*
 * mutex_init - Initialize a mutex.
 */
void
mutex_init(struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;

	atomic_store_explicit(&mp->owner, 0, memory_order_relaxed);
	mp->count = 0;
	event_init(&mp->event, "mutex", ev_LOCK);
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

	atomic_fetch_or_explicit(
	    &mp->owner,
	    MUTEX_WAITERS,
	    memory_order_relaxed
	);

	/* wait for unlock */
	int err;
	switch ((err = sch_sleep(&mp->event))) {
	case 0:
		/* mutex_unlock will set us as the owner */
		assert(mutex_owner(m) == thread_cur());
		break;
	case -EINTR:
#if defined(CONFIG_DEBUG)
		--thread_cur()->mutex_locks;
#endif
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

	/* wake up one waiter and set new owner */
	atomic_store_explicit(
	    &mp->owner,
	    (intptr_t)sch_wakeone(&mp->event),
	    memory_order_relaxed
	);
	mp->count = 1;
	assert(atomic_load_explicit(&mp->owner, memory_order_relaxed));

	if (event_waiting(&mp->event))
		atomic_fetch_or_explicit(&mp->owner, MUTEX_WAITERS, memory_order_relaxed);

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
 * mutex_count - get lock count of mutex
 */
unsigned
mutex_count(const struct mutex *m)
{
	struct mutex_private *mp = (struct mutex_private*)m->storage;
	return mp->count;
}

/*
 * mutex_assert_locked - ensure that current thread owns mutex
 */
void
mutex_assert_locked(const struct mutex *m)
{
	assert(mutex_owner(m) == thread_cur());
}
