#include <futex.h>

#include <access.h>
#include <arch.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <limits.h>
#include <sch.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <task.h>
#include <thread.h>
#include <vm.h>

#define trace(...)

/*
 * TODO: optimise search using hash map?
 * TODO: free unused futexes?
 */

/*
 * futex - kernel data for a userspace futex
 */
struct futex {
	phys *addr;		/* futex address */
	struct spinlock lock;	/* to synchronise operations on this futex */
	struct event event;	/* event */
	struct list link;	/* linkage on futexes list */
};

/*
 * futexes_impl - internal details of struct futexes
 */
struct futexes_impl {
	struct spinlock lock;
	struct list list;
};
static_assert(sizeof(struct futexes_impl) == sizeof(struct futexes), "");
static_assert(alignof(struct futexes_impl) == alignof(struct futexes), "");

/*
 * futexes - retrieve futexes_impl from task
 */
static struct futexes_impl *
futexes(struct task *t)
{
	return (struct futexes_impl *)task_futexes(t);
}

/*
 * futex_find - search for futex associated with uaddr
 */
static struct futex *
futex_find_unlocked(struct futexes_impl *fi, int *uaddr)
{
	struct futex *f;
	list_for_each_entry(f, &fi->list, link) {
		if (f->addr == virt_to_phys(uaddr))
			return f;
	}
	return 0;
}

static struct futex *
futex_find(struct futexes_impl *fi, int *uaddr)
{
	spinlock_lock(&fi->lock);
	struct futex *f = futex_find_unlocked(fi, uaddr);
	spinlock_unlock(&fi->lock);

	return f;
}

/*
 * futex_get - find or create futex for uaddr
 */
static struct futex *
futex_get(struct futexes_impl *fi, int *uaddr)
{
	struct futex *f;

	spinlock_lock(&fi->lock);

	if ((f = futex_find_unlocked(fi, uaddr)))
		goto out;

	if (!(f = malloc(sizeof(struct futex))))
		goto out;

	f->addr = virt_to_phys(uaddr);
	spinlock_init(&f->lock);
	event_init(&f->event, "futex", ev_LOCK);
	list_insert(&fi->list, &f->link);

out:
	spinlock_unlock(&fi->lock);
	return f;
}

/*
 * futex_wait - perform FUTEX_WAIT operation
 */
static int
futex_wait(struct task *t, int *uaddr, int val, const struct timespec *ts)
{
	int err, uval;

	if (ts && (ts->tv_sec < 0 || ts->tv_nsec > 1000000000))
		return -EINVAL;

	struct futex *f;
	if (!(f = futex_get(futexes(t), uaddr)))
		return DERR(-ENOMEM);

	if ((err = u_access_begin()) < 0)
		return err;
	if (!u_access_okfor(t->as, uaddr, 4, PROT_READ)) {
		u_access_end();
		return DERR(-EFAULT);
	}

	spinlock_lock(&f->lock);
	uval = atomic_load((uint32_t *)uaddr);
	u_access_end();

	if (uval != val) {
		spinlock_unlock(&f->lock);
		return -EAGAIN;
	}

	trace("futex_wait th:%p uaddr:%p val:%x ns:%llu\n",
	    thread_cur(), uaddr, val, ts ? ts_to_ns(ts) : 0);

	err = sch_prepare_sleep(&f->event, ts ? ts_to_ns(ts) : 0);
	spinlock_unlock(&f->lock);
	if (err)
		return err;
	return sch_continue_sleep();
	/* Be _very_ careful. Requeue can move us from one futex to another, so
	 * we are not necessarily waiting on 'f' anymore. */
}

/*
 * futex_wake - perform FUTEX_WAKE operation
 */
static int
futex_wake(struct task *t, int *uaddr, int val)
{
	trace("futex_wake th:%p uaddr:%p val:%d\n", thread_cur(), uaddr, val);

	if (val < 0)
		return DERR(-EINVAL);
	if (val == 0)
		return 0;

	int n;
	struct futex *f;
	if (!(f = futex_find(futexes(t), uaddr)))
		return 0;

	spinlock_lock(&f->lock);
	switch (val) {
	case 1:
		val = sch_wakeone(&f->event) ? 1 : 0;
		break;
	case INT_MAX:
		val = sch_wakeup(&f->event, 0);
		break;
	default:
		n = val;
		while (n && sch_wakeone(&f->event)) --n;
		val -= n;
	}
	spinlock_unlock(&f->lock);

	return val;
}

/*
 * futex_requeue - perform FUTEX_REQUEUE operation
 */
static int
futex_requeue(struct task *t, int *uaddr, int val, int val2, int *uaddr2)
{
	trace("futex_requeue th:%p uaddr:%p val:%d val2:%d uaddr2:%p\n",
	    thread_cur(), uaddr, val, val2, uaddr2);

	if (val < 0 || val2 < 0)
		return DERR(-EINVAL);

	struct futex *l;
	if (!(l = futex_find(futexes(t), uaddr)))
		return 0;

	spinlock_lock(&l->lock);

	int n = val;
	while (n && sch_wakeone(&l->event)) --n;

	spinlock_unlock(&l->lock);

	if (val2) {
		struct futex *r;
		if (!(r = futex_get(futexes(t), uaddr2)))
			return DERR(-ENOMEM);

		while (val2-- && sch_requeue(&l->event, &r->event));
	}

	return val - n;
}

/*
 * futex - kernel implementation of futex
 */
int
futex(struct task *t, int *uaddr, int op, int val, void *val2, int *uaddr2)
{
	assert(!interrupt_running());

	if ((op & FUTEX_OP_MASK) == FUTEX_REQUEUE && !u_addressfor(t->as, uaddr2))
		return DERR(-EFAULT);

	/* no support for realtime clock */
	if ((op & FUTEX_CLOCK_REALTIME))
		return DERR(-ENOSYS);

	if (!(op & FUTEX_PRIVATE))
		dbg("WARNING: shared futexes not yet supported\n");

	switch (op & FUTEX_OP_MASK) {
	case FUTEX_WAIT:
		assert(!sch_locks());
		return futex_wait(t, uaddr, val, val2);
	case FUTEX_WAKE:
		return futex_wake(t, uaddr, val);
	case FUTEX_REQUEUE:
		return futex_requeue(t, uaddr, val, (int)val2, uaddr2);
	/*
	 * TODO(FUTEX_LOCK_PI)
	 * TODO(FUTEX_UNOCK_PI)
	 */
	default:
		return DERR(-ENOTSUP);
	}
}

/*
 * sc_futex - futex system call
 */
int
sc_futex(int *uaddr, int op, int val, void *val2, int *uaddr2)
{
	int ret;
	struct timespec ts;

	/* copy in userspace timespec */
	switch (op & FUTEX_OP_MASK) {
	case FUTEX_WAIT:
		if (!val2)
			break;
		if ((ret = vm_read(task_cur()->as, &ts, val2, sizeof(ts))) < 0)
			return ret;
		val2 = &ts;
	}

	return futex(task_cur(), uaddr, op, val, val2, uaddr2);
}

/*
 * futexes_init - initialise futexes structure
 */
void
futexes_init(struct futexes *fs)
{
	struct futexes_impl *fi = (struct futexes_impl *)fs;

	spinlock_init(&fi->lock);
	list_init(&fi->list);
}

/*
 * futexes_destroy - destroy futexes structure
 */
void
futexes_destroy(struct futexes *fs)
{
	struct futexes_impl *fi = (struct futexes_impl *)fs;

	spinlock_lock(&fi->lock);
	struct list *n;
	for (n = list_first(&fi->list); n != &fi->list; n = list_next(n)) {
		struct futex *fk = list_entry(n, struct futex, link);
		free(fk);
	}
	spinlock_unlock(&fi->lock);
}
