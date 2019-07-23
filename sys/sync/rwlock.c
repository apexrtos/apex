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
	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	/* state < 0 while writing */
	err = wait_event_lock(p->event, p->state >= 0, &p->lock);
	if (!err)
		++p->state;

	spinlock_unlock(&p->lock);

	return err;
}

/*
 * rwlock_read_unlock
 */
int
rwlock_read_unlock(struct rwlock *o)
{

	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	assert(p->state > 0);

	/* if no more readers, signal any waiting writers */
	if (!--p->state)
		sch_wakeup(&p->event, 0);

	spinlock_unlock(&p->lock);

	return 0;
}

/*
 * rwlock_write_lock_interruptible
 */
int
rwlock_write_lock_interruptible(struct rwlock *o)
{
	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	/* state == 0, no writers or readers */
	err = wait_event_lock(p->event, p->state == 0, &p->lock);
	if (!err)
		--p->state;

	spinlock_unlock(&p->lock);

	return err;
}

/*
 * rwlock_write_unlock
 */
int
rwlock_write_unlock(struct rwlock *o)
{
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	spinlock_lock(&p->lock);

	assert(p->state < 0);

	/* signal any waiting readers or writers */
	if (!++p->state)
		sch_wakeup(&p->event, 0);

	spinlock_unlock(&p->lock);

	return 0;
}
