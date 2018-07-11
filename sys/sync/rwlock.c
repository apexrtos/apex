/*
 * rwlock.c - a simple read/write lock
 *
 * This is a very simple implementation with some limitations:
 * - Always wakes up all blocked writers when read lock is released
 * - Readers starve writers
 * - No priority inheritance
 *
 * However, it is still useful for cases where there are lots of readers and
 * few writers.
 *
 * REVISIT: reimplement this using atomics rather than mutex/cond.
 */

#include <sync.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdalign.h>
#include <thread.h>

struct rwlock_private {
	struct mutex mutex;
	struct cond cond;
	struct thread *writer;
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
	mutex_init(&p->mutex);
	cond_init(&p->cond);
	p->state = 0;
}

/*
 * rwlock_read_lock
 */
int
rwlock_read_lock(struct rwlock *o)
{
	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	/* ignore if we currently hold write lock */
	if (p->writer == thread_cur())
		return 0;

	if ((err = mutex_lock(&p->mutex)) < 0)
		return err;

	/* state < 0 while writing */
	while (p->state < 0) {
		if ((err = cond_wait(&p->cond, &p->mutex)) < 0) {
			mutex_unlock(&p->mutex);
			return err;
		}
	}

	++p->state;

	return mutex_unlock(&p->mutex);
}

/*
 * rwlock_read_unlock
 */
int
rwlock_read_unlock(struct rwlock *o)
{
	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	/* ignore if we currently hold write lock */
	if (p->writer == thread_cur())
		return 0;

	if (p->state <= 0)
		return DERR(-EINVAL);

	if ((err = mutex_lock(&p->mutex)) < 0)
		return err;

	/* if no more readers, signal any waiting writers */
	if (!--p->state)
		cond_broadcast(&p->cond);

	return mutex_unlock(&p->mutex);
}

/*
 * rwlock_write_lock
 */
int
rwlock_write_lock(struct rwlock *o)
{
	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	if ((err = mutex_lock(&p->mutex)) < 0)
		return err;

	if (p->writer != thread_cur()) {
		/* state == 0, no writers or readers */
		while (p->state) {
			if ((err = cond_wait(&p->cond, &p->mutex)) < 0) {
				mutex_unlock(&p->mutex);
				return err;
			}
		}

		p->writer = thread_cur();
	}

	--p->state;

	return mutex_unlock(&p->mutex);
}

/*
 * rwlock_write_unlock
 */
int
rwlock_write_unlock(struct rwlock *o)
{
	int err;
	struct rwlock_private *p = (struct rwlock_private *)o->storage;

	if (p->state >= 0)
		return DERR(-EINVAL);

	if ((err = mutex_lock(&p->mutex)) < 0)
		return err;

	/* signal any waiting readers or writers if no more recursion */
	if (!++p->state) {
		p->writer = 0;
		cond_broadcast(&p->cond);
	}

	return mutex_unlock(&p->mutex);
};
