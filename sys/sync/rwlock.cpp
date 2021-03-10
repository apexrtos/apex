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

#include <arch/interrupt.h>
#include <cassert>
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
static_assert(sizeof(rwlock_private) == sizeof(rwlock));
static_assert(alignof(rwlock_private) == alignof(rwlock));

/*
 * rwlock_init
 */
void
rwlock_init(rwlock *o)
{
	rwlock_private *p = (rwlock_private *)o->storage;
	event_init(&p->event, "rwlock", event::ev_LOCK);
	p->state = 0;
}

/*
 * rwlock_read_lock
 */
static int
rwlock_read_lock_s(rwlock *o, bool block_signals)
{
	assert(!interrupt_running());
	assert(!sch_locks());

	int err;
	rwlock_private *p = (rwlock_private *)o->storage;

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
rwlock_read_lock_interruptible(rwlock *o)
{
	return rwlock_read_lock_s(o, false);
}

/*
 * rwlock_read_lock - non interruptible read lock
 */
int
rwlock_read_lock(rwlock *o)
{
	return rwlock_read_lock_s(o, true);
}

/*
 * rwlock_read_unlock
 */
void
rwlock_read_unlock(rwlock *o)
{
	assert(!interrupt_running());

	rwlock_private *p = (rwlock_private *)o->storage;

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
rwlock_read_locked(rwlock *o)
{
	rwlock_private *p = (rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	bool r =  p->state > 0;

	return r;
}

/*
 * rwlock_write_lock
 */
static int
rwlock_write_lock_s(rwlock *o, bool block_signals)
{
	assert(!interrupt_running());
	assert(!sch_locks());

	int err;
	rwlock_private *p = (rwlock_private *)o->storage;

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
rwlock_write_lock_interruptible(rwlock *o)
{
	return rwlock_write_lock_s(o, false);
}

int
rwlock_write_lock(rwlock *o)
{
	return rwlock_write_lock_s(o, true);
}

/*
 * rwlock_write_unlock
 */
void
rwlock_write_unlock(rwlock *o)
{
	assert(!interrupt_running());

	rwlock_private *p = (rwlock_private *)o->storage;

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
rwlock_write_locked(rwlock *o)
{
	rwlock_private *p = (rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	return p->state < 0;
}

/*
 * rwlock_locked - test if rwlock is locked for reading or writing
 */
bool
rwlock_locked(rwlock *o)
{
	rwlock_private *p = (rwlock_private *)o->storage;

	std::lock_guard l{p->lock};

	return p->state != 0;
}
