#include <sync.h>

#include <arch/interrupt.h>
#include <atomic>
#include <cassert>
#include <climits>
#include <errno.h>
#include <event.h>
#include <sch.h>
#include <wait.h>

struct private {
	std::atomic_int count;
	struct event event;
};
static_assert(sizeof(private) == sizeof(semaphore), "");
static_assert(alignof(private) == alignof(semaphore), "");

/*
 * semaphore_init - initialise semaphore.
 */
void
semaphore_init(semaphore *s)
{
	private *p = (private *)s->storage;

	event_init(&p->event, "semaphore", ev_LOCK);
	p->count = 0;
}

/*
 * semaphore_post - increment (unlock) semaphore.
 *
 * Safe to call from interrupt context.
 */
int
semaphore_post(semaphore *s)
{
	private *p = (private *)s->storage;

	if (p->count == INT_MAX)
		return -EOVERFLOW;

	++p->count;
	sch_wakeone(&p->event);
	return 0;
}

/*
 * semaphore_post_once - increment (unlock) semaphore if it is not unlocked.
 *
 * This can be used to implement a binary semaphore.
 *
 * Safe to call from interrupt context.
 */
int
semaphore_post_once(semaphore *s)
{
	private *p = (private *)s->storage;

	if (p->count)
		return 0;

	++p->count;
	sch_wakeone(&p->event);
	return 0;
}

/*
 * semaphore_wait - decrement (lock) semaphore.
 */
int
semaphore_wait_interruptible(semaphore *s)
{
	assert(!sch_locks());
	assert(!interrupt_running());

	int r;
	private *p = (private *)s->storage;

	if ((r = wait_event_interruptible(p->event, [p] { return p->count > 0; })) < 0)
		return r;
	--p->count;
	assert(p->count >= 0);
	return 0;
}
