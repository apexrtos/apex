/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <conf/config.h>
#include <errno.h>
#include <mutex>
#include <types.h>

struct thread;

#define MUTEX_WAITERS	0x00000001
#define MUTEX_RECURSIVE	0x00000002
#define MUTEX_TID_MASK	0xFFFFFFFC

struct cond {
	union {
		char storage[16];
		unsigned align;
	};
};

struct mutex {
	union {
		char storage[28];
		unsigned align;
	};
};

struct rwlock {
	union {
		char storage[24];
		unsigned align;
	};
};

struct spinlock {
#if defined(CONFIG_SMP)
#error not yet implemented
#elif defined CONFIG_DEBUG
	thread *owner;
#else
	char dummy;
#endif
};

struct semaphore {
	union {
		char storage[20];
		unsigned align;
	};
};

bool	       mutex_valid(const mutex *);
void	       mutex_init(mutex *);
int	       mutex_lock_interruptible(mutex *);
int	       mutex_lock(mutex *);
int	       mutex_unlock(mutex *);
thread *mutex_owner(const mutex *);
void	       mutex_assert_locked(const mutex *);

bool	       cond_valid(const cond *);
void	       cond_init(cond *);
int	       cond_wait_interruptible(cond *, mutex *);
int	       cond_wait(cond *, mutex *);
int cond_timedwait_interruptible(cond *, mutex *, uint_fast64_t);
int	       cond_timedwait(cond *, mutex *, uint_fast64_t);
int	       cond_signal(cond *);
int	       cond_broadcast(cond *);

void	       rwlock_init(rwlock *);
int	       rwlock_read_lock_interruptible(rwlock *);
int	       rwlock_read_lock(rwlock *);
void	       rwlock_read_unlock(rwlock *);
bool	       rwlock_read_locked(rwlock *);
int	       rwlock_write_lock_interruptible(rwlock *);
int	       rwlock_write_lock(rwlock *);
void	       rwlock_write_unlock(rwlock *);
bool	       rwlock_write_locked(rwlock *);
bool	       rwlock_locked(rwlock *);

void	       spinlock_init(spinlock *);
void	       spinlock_lock(spinlock *);
void	       spinlock_unlock(spinlock *);
int	       spinlock_lock_irq_disable(spinlock *);
void	       spinlock_unlock_irq_restore(spinlock *, int);
void	       spinlock_assert_locked(const spinlock *);

void	       semaphore_init(semaphore *);
int	       semaphore_post(semaphore *);
int	       semaphore_post_once(semaphore *);
int	       semaphore_wait_interruptible(semaphore *);

namespace a {

/*
 * a::mutex - Apex c++ mutex wrapper
 */
class mutex final {
public:
	mutex() { mutex_init(&m_); }
	mutex(mutex &&) = delete;
	mutex(const mutex &) = delete;
	mutex &operator=(mutex &&) = delete;
	mutex &operator=(const mutex &) = delete;
	int interruptible_lock() { return mutex_lock_interruptible(&m_); }
	int lock() { return mutex_lock(&m_); }
	int unlock() { return mutex_unlock(&m_); }
	void assert_locked() const { mutex_assert_locked(&m_); }
	::mutex *native_handle() { return &m_; }

private:
	::mutex m_;
};

/*
 * a::condition_variable - Apex c++ condition variable wrapper
 */
class condition_variable final {
public:
	condition_variable() { cond_init(&c_); }
	condition_variable(condition_variable &&) = delete;
	condition_variable(const condition_variable &) = delete;
	condition_variable &operator=(condition_variable &&) = delete;
	condition_variable &operator=(const condition_variable &) = delete;

	void
	wait(std::unique_lock<mutex> &m)
	{
		cond_wait(&c_, m.mutex()->native_handle());
	}

	int
	wait(std::unique_lock<mutex> &m, const auto &pred)
	{
		while (!pred())
			wait(m);
		return 0;
	}

	int
	wait_interruptible(std::unique_lock<mutex> &m)
	{
		return cond_wait_interruptible(&c_, m.mutex()->native_handle());
	}

	int
	wait_interruptible(std::unique_lock<mutex> &m, const auto &pred)
	{
		while (!pred()) {
			if (wait_interruptible(m) == -EINTR)
				return -EINTR;
		}
		return 0;
	}

	void notify_one() { cond_signal(&c_); }
	void notify_all() { cond_broadcast(&c_); }

private:
	::cond c_;
};

/*
 * a::rwlock - Apex c++ rwlock wrapper
 */
class rwlock_read;
class rwlock_write;
class rwlock {
public:
	rwlock() { rwlock_init(&m_); }
	rwlock(rwlock &&) = delete;
	rwlock(const rwlock &) = delete;
	rwlock &operator=(rwlock &&) = delete;
	rwlock &operator=(const rwlock &) = delete;
	rwlock_read &read();
	rwlock_write &write();
	bool locked() { return rwlock_locked(&m_); }

protected:
	::rwlock m_;
};
class rwlock_read final : public rwlock {
public:
	int lock() { return rwlock_read_lock(&m_); }
	int interruptible_lock() { return rwlock_read_lock_interruptible(&m_); }
	void unlock() { rwlock_read_unlock(&m_); }
	bool locked() { return rwlock_read_locked(&m_); }
};
class rwlock_write final : public rwlock {
public:
	int lock() { return rwlock_write_lock(&m_); }
	int interruptible_lock() { return rwlock_write_lock_interruptible(&m_); }
	void unlock() { rwlock_write_unlock(&m_); }
	bool locked() { return rwlock_write_locked(&m_); }
};
inline rwlock_read &rwlock::read() { return static_cast<rwlock_read &>(*this); }
inline rwlock_write &rwlock::write() { return static_cast<rwlock_write &>(*this); }

/*
 * a::spinlock - Apex c++ spinlock wrapper
 */
class spinlock final {
public:
	spinlock() { spinlock_init(&s_); }
	spinlock(spinlock &&) = delete;
	spinlock(const spinlock &) = delete;
	spinlock &operator=(spinlock &&) = delete;
	spinlock &operator=(const spinlock &) = delete;
	void lock() { spinlock_lock(&s_); }
	void unlock() { spinlock_unlock(&s_); }
	void assert_locked() const { spinlock_assert_locked(&s_); }

private:
	::spinlock s_;
};

/*
 * a::spinlock_irq - Apex c++ irq disabling spinlock wrapper
 */
class spinlock_irq final {
public:
	spinlock_irq() { spinlock_init(&s_); }
	spinlock_irq(spinlock_irq &&) = delete;
	spinlock_irq(const spinlock_irq &) = delete;
	spinlock_irq &operator=(spinlock_irq &&) = delete;
	spinlock_irq &operator=(const spinlock_irq &) = delete;
	int lock() { return spinlock_lock_irq_disable(&s_); }
	void unlock(int v) { spinlock_unlock_irq_restore(&s_, v); }
	void assert_locked() const { spinlock_assert_locked(&s_); }

private:
	::spinlock s_;
};

/*
 * a::semaphore - Apex c++ semaphore wrapper
 */
class semaphore final {
public:
	semaphore() { semaphore_init(&s_); }
	semaphore(semaphore &&) = delete;
	semaphore(const semaphore &) = delete;
	semaphore &operator=(semaphore &&) = delete;
	semaphore &operator=(const semaphore &) = delete;
	int post() { return semaphore_post(&s_); }
	int post_once() { return semaphore_post_once(&s_); }
	int wait_interruptible() { return semaphore_wait_interruptible(&s_); }

private:
	::semaphore s_;
};

} /* namespace a */

namespace std {

/*
 * std::lock_guard specialisation for a::spinlock_irq
 */
template<>
class lock_guard<a::spinlock_irq> {
public:
	typedef a::spinlock_irq mutex_type;

	explicit lock_guard(a::spinlock_irq &m)
	: m_{m}
	{
		s = m_.lock();
	}

	~lock_guard()
	{
		m_.unlock(s);
	}

	lock_guard(lock_guard &&) = delete;
	lock_guard(const lock_guard &) = delete;
	lock_guard &operator=(lock_guard &&) = delete;
	lock_guard &operator=(const lock_guard &) = delete;

private:
	a::spinlock_irq &m_;
	int s;
};

/*
 * std::unique_lock specialisation for a::spinlock_irq
 */
template<>
class unique_lock<a::spinlock_irq> {
public:
	typedef a::spinlock_irq mutex_type;

	unique_lock()
	: m_{0}
	, locked_{false}
	{ }

	explicit unique_lock(a::spinlock_irq &m)
	: m_{&m}
	, locked_{false}
	{
		lock();
	}

	unique_lock(a::spinlock_irq &m, defer_lock_t)
	: m_{&m}
	, locked_{false}
	{ }

	~unique_lock()
	{
		if (locked_)
			unlock();
	}

	unique_lock(const unique_lock &) = delete;
	unique_lock &operator=(const unique_lock &) = delete;

	unique_lock(unique_lock &&o)
	: m_{o.m_}
	, locked_{o.locked_}
	, s_{o.s_}
	{
		o.m_ = nullptr;
		o.locked_ = false;
	}

	unique_lock &operator=(unique_lock &&o)
	{
		if (locked_)
			unlock();

		unique_lock tmp{std::move(o)};
		tmp.swap(*this);

		return *this;
	}

	void lock()
	{
		assert(m_);
		assert(!locked_);
		s_ = m_->lock();
		locked_ = true;
	}

	void unlock()
	{
		assert(m_);
		assert(locked_);
		m_->unlock(s_);
		locked_ = false;
	}

	void swap(unique_lock &o)
	{
		std::swap(m_, o.m_);
		std::swap(s_, o.s_);
		std::swap(locked_, o.locked_);
	}

	a::spinlock_irq* release()
	{
		const auto r{m_};
		m_ = 0;
		locked_ = false;
		return r;
	}

	bool owns_lock() const
	{
		return locked_;
	}

	explicit operator bool() const
	{
		return locked_;
	}

	a::spinlock_irq* mutex() const
	{
		return m_;
	}

private:
	a::spinlock_irq *m_;
	bool locked_;
	int s_;
};

} /* namespace std */

/*
 * interruptible_lock
 */
template<typename T>
class interruptible_lock {
public:
	typedef T mutex_type;

	explicit interruptible_lock(T &m)
	: m_{m}
	, locked_{false}
	{ }

	auto lock()
	{
		assert(!locked_);

		/* m_.interruptible_lock() returns 0 on success */
		const auto r = m_.interruptible_lock();
		locked_ = !r;
		return r;
	}

	void unlock()
	{
		if (locked_) {
			m_.unlock();
			locked_ = false;
		}
	}

	bool locked() const
	{
		return locked_;
	}

	~interruptible_lock()
	{
		unlock();
	}

	interruptible_lock(interruptible_lock &&) = delete;
	interruptible_lock(const interruptible_lock &) = delete;
	interruptible_lock &operator=(interruptible_lock &&) = delete;
	interruptible_lock &operator=(const interruptible_lock &) = delete;

private:
	T &m_;
	bool locked_;
};
