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
 * cond.c - condition variable object
 */

#include <sync.h>

#include <access.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <sch.h>
#include <stdalign.h>
#include <stddef.h>
#include <thread.h>
#include <timer.h>

#define COND_MAGIC     0x436f6e3f      /* 'Con?' */

struct cond_private {
	int magic;		/* magic number */
	struct mutex *mutex;	/* mutex associated with this condition */
	unsigned wait;		/* # waiting threads */
	unsigned signal;	/* # of signals */
	struct event event;	/* event */
};

static_assert(sizeof(struct cond_private) == sizeof(struct cond), "");
static_assert(alignof(struct cond_private) == alignof(struct cond), "");

/*
 * cond_valid - check cond validity.
 */
bool
cond_valid(const struct cond *c)
{
	const struct cond_private *cp = (struct cond_private*)c->storage;

	return k_address(c) && cp->magic == COND_MAGIC;
}

/*
 * cond_init - Create and initialize a condition variable.
 *
 * If an initialized condition variable is reinitialized,
 * undefined behavior results.
 */
void
cond_init(struct cond *c)
{
	struct cond_private *cp = (struct cond_private*)c->storage;

	cp->magic = COND_MAGIC;
	cp->mutex = NULL;
	cp->wait = 0;
	cp->signal = 0;
	event_init(&cp->event, "condition", ev_COND);
}

/*
 * cond_wait_interruptible - Wait on a condition for ever.
 */
int
cond_wait_interruptible(struct cond *c, struct mutex *m)
{
	return cond_timedwait_interruptible(c, m, 0);
}

/*
 * cond_timedwait_interruptible - Wait on a condition for a specified time.
 *
 * If the thread receives any exception while waiting CV, this
 * routine returns immediately with EINTR.
 */
int
cond_timedwait_interruptible(struct cond *c, struct mutex *m, uint64_t nsec)
{
	if (!cond_valid(c))
		return DERR(-EINVAL);

	struct cond_private *cp = (struct cond_private*)c->storage;

	/* can't wait with recursively locked mutex */
	if (mutex_count(m) > 1)
		return DERR(-EDEADLK);

	int rc, err = 0;

	sch_lock();

	/* multiple threads can't wait with different mutexes */
	if ((cp->mutex != NULL) && (cp->mutex != m)) {
		sch_unlock();
		return DERR(-EINVAL);
	}

	/* unlock mutex and store */
	mutex_unlock(m);
	cp->mutex = m;

	/* wait for signal */
	++cp->wait;
	rc = sch_nanosleep(&cp->event, nsec);
	--cp->wait;

	/* reacquire mutex */
	if ((err = mutex_lock_interruptible(m)))
		goto out;

	/* received signal */
	if (cp->signal) {
		--cp->signal;
		goto out;
	}

	/* sleep timed out or was interrupted */
	assert(rc < 0);
	err = rc;

out:
	sch_unlock();
	return err;
}

/*
 * cond_signal - Unblock one thread that is blocked on the specified CV.
 * The thread which has highest priority will be unblocked.
 */
int
cond_signal(struct cond *c)
{
	if (!cond_valid(c))
		return DERR(-EINVAL);

	struct cond_private *cp = (struct cond_private*)c->storage;

	sch_lock();
	if (cp->signal < cp->wait) {
		++cp->signal;
		sch_wakeone(&cp->event);
	}
	sch_unlock();

	return 0;
}

/*
 * cond_broadcast - Unblock all threads that are blocked on the specified CV.
 */
int
cond_broadcast(struct cond *c)
{
	if (!cond_valid(c))
		return DERR(-EINVAL);

	struct cond_private *cp = (struct cond_private*)c->storage;

	sch_lock();
	if (cp->signal < cp->wait) {
		cp->signal = cp->wait;
		sch_wakeup(&cp->event, 0);
	}
	sch_unlock();

	return 0;
}

