#pragma once

#include <cassert>
#include <sch.h>
#include <sig.h>
#include <thread.h>
#include <timer.h>

/*
 * wait_event - wait for an event.
 */
int
wait_event_interruptible(event &e, auto condition)
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

int
wait_event(event &e, auto condition)
{
	const k_sigset_t sig_mask = sig_block_all();
	const int r = wait_event_interruptible(e, condition);
	sig_restore(&sig_mask);
	return r;
}

/*
 * wait_event_timeout - wait for an event with timeout.
 *
 * Returns nanoseconds of sleep time remaining, or -ve on timeout or interrupt.
 *
 * ns = 0 disables timeout.
 */
int
wait_event_interruptible_timeout(event &e, int ns, auto condition)
{
	assert(ns >= 0);
	const uint_fast64_t expire = timer_monotonic_coarse() + ns;
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

int
wait_event_timeout(event &e, int ns, auto condition)
{
	const k_sigset_t sig_mask = sig_block_all();
	const int r = wait_event_interruptible_timeout(e, ns, condition);
	sig_restore(&sig_mask);
	return r;
}

/*
 * wait_event_lock - wait for an event which must be tested with lock held.
 */
int
wait_event_interruptible_lock(event &e, auto &lock, auto condition)
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

int
wait_event_lock(event &e, auto &lock, auto condition)
{
	const k_sigset_t sig_mask = sig_block_all();
	const int r = wait_event_interruptible_lock(e, lock, condition);
	sig_restore(&sig_mask);
	return r;
}
