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
	struct spinlock lock;
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
	spinlock_init(&p->lock);
	event_init(&p->event, "rwlock", ev_LOCK);
	p->state = 0;
}

/*
 * rwlock_read_lock_interruptible
 */
int
rwlock_read_lock_interruptible(struct rwlock *o)
{
	assert(!interrupt_running());
	assert(!sch_locks());

	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	/* state < 0 while writing */
	err = wait_event_interruptible_lock(p->event, p->state >= 0, &p->lock);
	if (!err) {
		++p->state;
#if defined(CONFIG_DEBUG)
		++thread_cur()->rwlock_locks;
#endif
	}

	spinlock_unlock(&p->lock);

	return err;
}

/*
 * rwlock_read_unlock
 */
void
rwlock_read_unlock(struct rwlock *o)
{
	assert(!interrupt_running());

	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	assert(p->state > 0);

	/* if no more readers, signal any waiting writers */
	if (!--p->state)
		sch_wakeup(&p->event, 0);
#if defined(CONFIG_DEBUG)
	--thread_cur()->rwlock_locks;
#endif
	spinlock_unlock(&p->lock);
}

/*
 * rwlock_read_locked
 */
bool
rwlock_read_locked(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	bool r =  p->state > 0;

	spinlock_unlock(&p->lock);

	return r;
}

/*
 * rwlock_write_lock_interruptible
 */
int
rwlock_write_lock_interruptible(struct rwlock *o)
{
	assert(!interrupt_running());
	assert(!sch_locks());

	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	/* state == 0, no writers or readers */
	err = wait_event_interruptible_lock(p->event, p->state == 0, &p->lock);
	if (!err) {
		--p->state;
#if defined(CONFIG_DEBUG)
		++thread_cur()->rwlock_locks;
#endif
	}

	spinlock_unlock(&p->lock);

	return err;
}

/*
 * rwlock_write_unlock
 */
void
rwlock_write_unlock(struct rwlock *o)
{
	assert(!interrupt_running());

	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	assert(p->state < 0);

	/* signal any waiting readers or writers */
	if (!++p->state)
		sch_wakeup(&p->event, 0);
#if defined(CONFIG_DEBUG)
	--thread_cur()->rwlock_locks;
#endif
	spinlock_unlock(&p->lock);
}

/*
 * rwlock_write_locked
 */
bool
rwlock_write_locked(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	bool r =  p->state < 0;

	spinlock_unlock(&p->lock);

	return r;
}
