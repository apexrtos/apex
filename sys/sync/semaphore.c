#include <sync.h>

#include <assert.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <sch.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <wait.h>

struct private {
	atomic_int count;
	struct event event;
};
static_assert(sizeof(struct private) == sizeof(struct semaphore), "");
static_assert(alignof(struct private) == alignof(struct semaphore), "");

/*
 * semaphore_init - initialise semaphore.
 */
void
semaphore_init(struct semaphore *s)
{
	struct private *p = (struct private *)s->storage;

	event_init(&p->event, "semaphore", ev_LOCK);
	p->count = 0;
}

/*
 * semaphore_post - increment (unlock) semaphore.
 *
 * Safe to call from interrupt context.
 */
int
semaphore_post(struct semaphore *s)
{
	struct private *p = (struct private *)s->storage;

	if (p->count == INT_MAX)
		return -EOVERFLOW;

	++p->count;
	sch_wakeone(&p->event);
	return 0;
}

/*
 * semaphore_wait - decrenent (lock) semaphore.
 */
int
semaphore_wait_interruptible(struct semaphore *s)
{
	int r;
	struct private *p = (struct private *)s->storage;

	if ((r = wait_event(p->event, p->count > 0)) < 0)
		return r;
	--p->count;
	assert(p->count >= 0);
	return 0;
}
