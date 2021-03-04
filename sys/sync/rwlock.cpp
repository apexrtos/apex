/*
 * rwlock.c - a simple read/write lock
 *
 * This is a very simple implementation with some limitations:
 * - Always wakes up all blocked writers when read lock is released
 * - Readers starve writers
 * - No priority inheritance
 * - Read lock cannot be upgraded to write lock
 * - Write lock is not recursive
 *
 * However, it is still useful for cases where there are lots of readers and
 * few writers.
 *
 * REVISIT: optimise using atomic variable for state.
 *	    SMP: spin before going to sleep.
 */

#include <sync.h>

#include <arch.h>
#include <assert.h>
#include <stdalign.h>
#include <wait.h>

struct rwlock_private {
	a::spinlock lock;
	struct event event;
	/*
	 * state == 0, unlocked
	 * state > 0, reading
	 * state < 0, writing
	 */
	int state;
};
static_assert(sizeof(struct rwlock_private) == sizeof(struct rwlock), "");
static_assert(alignof(struct rwlock_private) == alignof(struct rwlock), "");

/*
 * rwlock_init
 */
void
rwlock_init(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;
	event_init(&p->event, "rwlock", ev_LOCK);
	p->state = 0;
}

/*
 * rwlock_read_lock
 */
static int
rwlock_read_lock_s(struct rwlock *o, bool block_signals)
{
	assert(!interrupt_running());
	assert(!sch_locks());

	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	if (!block_signals && sig_unblocked_pending(thread_cur()))
		return -EINTR;

	std::unique_lock l{p->lock};

	/* state < 0 while writing */
	auto cond = [p] { return p->state >= 0; };
	if (block_signals)
		err = wait_event_lock(p->event, l, cond);
	else
		err = wait_event_interruptible_lock(p->event, l, cond);
	if (!err) {
		++p->state;
#if defined(CONFIG_DEBUG)
		++thread_cur()->rwlock_locks;
#endif
	}

	return err;
}

/*
 * rwlock_read_lock_interruptible
 */
int
rwlock_read_lock_interruptible(struct rwlock *o)
{
	return rwlock_read_lock_s(o, false);
}

/*
 * rwlock_read_lock - non interruptible read lock
 */
int
rwlock_read_lock(struct rwlock *o)
{
	return rwlock_read_lock_s(o, true);
}

/*
 * rwlock_read_unlock
 */
void
rwlock_read_unlock(struct rwlock *o)
{
	assert(!interrupt_running());

	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	assert(p->state > 0);

	/* if no more readers, signal any waiting writers */
	if (!--p->state)
		sch_wakeup(&p->event, 0);
#if defined(CONFIG_DEBUG)
	--thread_cur()->rwlock_locks;
#endif
}

/*
 * rwlock_read_locked
 */
bool
rwlock_read_locked(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	bool r =  p->state > 0;

	return r;
}

/*
 * rwlock_write_lock
 */
static int
rwlock_write_lock_s(struct rwlock *o, bool block_signals)
{
	assert(!interrupt_running());
	assert(!sch_locks());

	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	if (!block_signals && sig_unblocked_pending(thread_cur()))
		return -EINTR;

	std::unique_lock l{p->lock};

	/* state == 0, no writers or readers */
	auto cond = [p] { return p->state == 0; };
	if (block_signals)
		err = wait_event_lock(p->event, l, cond);
	else
		err = wait_event_interruptible_lock(p->event, l, cond);
	if (!err) {
		--p->state;
#if defined(CONFIG_DEBUG)
		++thread_cur()->rwlock_locks;
#endif
	}

	return err;
}

int
rwlock_write_lock_interruptible(struct rwlock *o)
{
	return rwlock_write_lock_s(o, false);
}

int
rwlock_write_lock(struct rwlock *o)
{
	return rwlock_write_lock_s(o, true);
}

/*
 * rwlock_write_unlock
 */
void
rwlock_write_unlock(struct rwlock *o)
{
	assert(!interrupt_running());

	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	assert(p->state < 0);

	/* signal any waiting readers or writers */
	if (!++p->state)
		sch_wakeup(&p->event, 0);
#if defined(CONFIG_DEBUG)
	--thread_cur()->rwlock_locks;
#endif
}

/*
 * rwlock_write_locked
 */
bool
rwlock_write_locked(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	return p->state < 0;
}

/*
 * rwlock_locked - test if rwlock is locked for reading or writing
 */
bool
rwlock_locked(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	return p->state != 0;
}
