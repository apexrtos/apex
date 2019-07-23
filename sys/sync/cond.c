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

#include <arch.h>
#include <assert.h>
#include <event.h>
#include <sch.h>
#include <stdalign.h>

struct cond_private {
	struct event event;
};

static_assert(sizeof(struct cond_private) == sizeof(struct cond), "");
static_assert(alignof(struct cond_private) == alignof(struct cond), "");

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
 * If a signal is received while waiting on the condition EINTR will be
 * returned.
 */
int
cond_timedwait_interruptible(struct cond *c, struct mutex *m, uint64_t nsec)
{
	assert(!sch_locks());
	assert(!interrupt_running());

	struct cond_private *cp = (struct cond_private*)c->storage;

	int r;

	/* wait for signal or broadcast */
	if ((r = sch_prepare_sleep(&cp->event, nsec)) < 0)
		return r;
	mutex_unlock(m);
	r = sch_continue_sleep();
	mutex_lock(m);
	return r;
}

/*
 * cond_signal - Unblock one thread that is blocked on the specified CV.
 *
 * The thread which has highest priority will be unblocked.
 */
int
cond_signal(struct cond *c)
{
	struct cond_private *cp = (struct cond_private*)c->storage;

	sch_wakeone(&cp->event);
	return 0;
}

/*
 * cond_broadcast - Unblock all threads that are blocked on the specified CV.
 */
int
cond_broadcast(struct cond *c)
{
	struct cond_private *cp = (struct cond_private*)c->storage;

	sch_wakeup(&cp->event, 0);
	return 0;
}

