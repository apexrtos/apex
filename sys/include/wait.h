#ifndef wait_h
#define wait_h

#include <assert.h>
#include <sch.h>
#include <thread.h>
#include <timer.h>

#if defined(__cplusplus)

/*
 * wait_event - wait for an event.
 */
int
wait_event(event &e, auto condition)
{
	while (!condition()) {
		if (auto r = sch_prepare_sleep(&e, 0); r)
			return r;
		if (condition()) {
			sch_cancel_sleep();
			return 0;
		}
		if (auto r = sch_continue_sleep(); r)
			return r;
	}
	return 0;
}

/*
 * wait_event_timeout - wait for an event with timeout.
 *
 * Returns nanoseconds of sleep time remaining, or -ve on timeout or interrupt.
 */
int
wait_event_timeout(event &e, int ns, auto condition)
{
	assert(ns > 0);
	const uint64_t expire = timer_monotonic_coarse() + ns;
	while (!condition()) {
		if (auto r = sch_prepare_sleep(&e, ns); r)
			return r;
		if (condition()) {
			sch_cancel_sleep();
			return ns;
		}
		if (auto r = sch_continue_sleep(); r)
			return r;
		ns = expire - timer_monotonic_coarse();
		if (ns <= 0)
			ns = 1;
	}
	return ns;
}

/*
 * wait_event_lock - wait for an event which must be tested with lock held.
 */
int
wait_event_lock(event &e, auto &lock, auto condition)
{
	assert(lock.owns_lock());
	while (!condition()) {
		if (auto r = sch_prepare_sleep(&e, 0); r)
			return r;
		lock.unlock();
		auto r = sch_continue_sleep();
		lock.lock();
		if (r)
			return r;
	}
	return 0;
}

#else

/*
 * wait_event - wait for an event.
 */
#define wait_event(event, condition) \
({ \
	int __ret = 0; \
	while (!(condition) && !__ret) { \
		if ((__ret = sch_prepare_sleep(&event, 0))) \
			break; \
		if (condition) {  \
			sch_cancel_sleep(); \
			break; \
		} \
		__ret = sch_continue_sleep(); \
	} \
	__ret; \
})

/*
 * wait_event_timeout - wait for an event with timeout.
 *
 * Returns nanoseconds of sleep time remaining, or -ve on timeout or interrupt.
 */
#define wait_event_timeout(event, condition, ns) \
({ \
	int __rc, __ret = ns; \
	assert(__ret > 0); \
	const uint64_t __expire = timer_monotonic_coarse() + __ret; \
	while (!(condition)) { \
		if ((__rc = sch_prepare_sleep(&event, __ret))) { \
			__ret = __rc; \
			break; \
		} \
		if (condition) { \
			sch_cancel_sleep(); \
			break; \
		} \
		if ((__rc = sch_continue_sleep())) { \
			__ret = __rc; \
			break; \
		} \
		__ret = __expire - timer_monotonic_coarse(); \
		if (__ret <= 0) \
			__ret = 1; \
	} \
	__ret; \
})

/*
 * wait_event_lock - wait for an event which must be tested with lock held.
 */
#define wait_event_lock(event, condition, lock) \
({ \
	lock_assert_locked(lock); \
	int __ret = 0; \
	while (!(condition) && !__ret) { \
		if ((__ret = sch_prepare_sleep(&event, 0))) \
			break; \
		spinlock_unlock(lock); \
		__ret = sch_continue_sleep(); \
		spinlock_lock(lock); \
	} \
	__ret; \
})

/*
 * wait_event_lock_irq - wait for an event which must be tested with lock held.
 */
#define wait_event_lock_irq(event, condition, lock, irq_state) \
({ \
	spinlock_assert_locked(lock); \
	int __ret = 0; \
	while (!(condition) && !__ret) { \
		if ((__ret = sch_prepare_sleep(&event, 0))) \
			break; \
		spinlock_unlock_irq_restore(lock, irq_state); \
		__ret = sch_continue_sleep(); \
		irq_state = spinlock_lock_irq_disable(lock); \
	} \
	__ret; \
})

#endif

#endif
