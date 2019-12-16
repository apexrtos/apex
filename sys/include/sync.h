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

#ifndef sync_h
#define sync_h

#include <conf/config.h>
#include <types.h>

#if defined(__cplusplus)
#include <mutex>
#endif

struct list;
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
	struct thread *owner;
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

#if defined(__cplusplus)
extern "C" {
#endif

bool	       mutex_valid(const struct mutex *);
void	       mutex_init(struct mutex *);
int	       mutex_lock_interruptible(struct mutex *);
int	       mutex_lock(struct mutex *);
int	       mutex_unlock(struct mutex *);
struct thread *mutex_owner(const struct mutex *);
void	       mutex_assert_locked(const struct mutex *);

bool	       cond_valid(const struct cond *);
void	       cond_init(struct cond *);
int	       cond_wait_interruptible(struct cond *, struct mutex *);
int	       cond_timedwait_interruptible(struct cond *, struct mutex *,
					    uint_fast64_t);
int	       cond_signal(struct cond *);
int	       cond_broadcast(struct cond *);

void	       rwlock_init(struct rwlock *);
int	       rwlock_read_lock_interruptible(struct rwlock *);
void	       rwlock_read_unlock(struct rwlock *);
bool	       rwlock_read_locked(struct rwlock *);
int	       rwlock_write_lock_interruptible(struct rwlock *);
void	       rwlock_write_unlock(struct rwlock *);
bool	       rwlock_write_locked(struct rwlock *);

void	       spinlock_init(struct spinlock *);
void	       spinlock_lock(struct spinlock *);
void	       spinlock_unlock(struct spinlock *);
int	       spinlock_lock_irq_disable(struct spinlock *);
void	       spinlock_unlock_irq_restore(struct spinlock *, int);
void	       spinlock_assert_locked(const struct spinlock *);

void	       semaphore_init(struct semaphore *);
int	       semaphore_post(struct semaphore *);
int	       semaphore_wait_interruptible(struct semaphore *);

#if defined(__cplusplus)
} /* extern "C" */

namespace a {

/*
 * a::mutex - apex c++ mutex wrapper
 */
class mutex {
public:
	mutex() { mutex_init(&m_); }
	int interruptible_lock() { return mutex_lock_interruptible(&m_); }
	int lock() { return mutex_lock(&m_); }
	int unlock() { return mutex_unlock(&m_); }
	void assert_locked() const { mutex_assert_locked(&m_); }

private:
	::mutex m_;
};

/*
 * a::rwlock - apex c++ rwlock wrapper
 */
class rwlock_read;
class rwlock_write;
class rwlock {
public:
	rwlock() { rwlock_init(&m_); }
	rwlock_read &read();
	rwlock_write &write();

protected:
	::rwlock m_;
};
class rwlock_read : public rwlock {
public:
	int interruptible_lock() { return rwlock_read_lock_interruptible(&m_); }
	void unlock() { rwlock_read_unlock(&m_); }
	bool locked() { return rwlock_read_locked(&m_); }
};
class rwlock_write : public rwlock {
public:
	int interruptible_lock() { return rwlock_write_lock_interruptible(&m_); }
	void unlock() { rwlock_write_unlock(&m_); }
	bool locked() { return rwlock_write_locked(&m_); }
};
inline rwlock_read &rwlock::read() { return static_cast<rwlock_read&>(*this); }
inline rwlock_write &rwlock::write() { return static_cast<rwlock_write&>(*this); }

/*
 * a::spinlock - apex c++ spinlock wrapper
 */
class spinlock {
public:
	spinlock() { spinlock_init(&s_); }
	void lock() { spinlock_lock(&s_); }
	void unlock() { spinlock_unlock(&s_); }
	void assert_locked() const { spinlock_assert_locked(&s_); }

private:
	::spinlock s_;
};

/*
 * a::spinlock_irq - apex c++ irq disabling spinlock wrapper
 */
class spinlock_irq {
public:
	spinlock_irq() { spinlock_init(&s_); }
	int lock() { return spinlock_lock_irq_disable(&s_); }
	void unlock(int v) { spinlock_unlock_irq_restore(&s_, v); }
	void assert_locked() const { spinlock_assert_locked(&s_); }

private:
	::spinlock s_;
};

/*
 * a::semaphore - apex c++ semaphore wrapper
 */
class semaphore {
public:
	semaphore() { semaphore_init(&s_); }
	int post() { return semaphore_post(&s_); }
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

	lock_guard(const lock_guard&) = delete;
	lock_guard&operator=(const lock_guard&) = delete;

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

	unique_lock(const unique_lock&) = delete;
	unique_lock&operator=(const unique_lock&) = delete;

	unique_lock(unique_lock&&o)
	: m_{o.m_}
	, locked_{o.locked_}
	, s_{o.s_}
	{
		o.m_ = nullptr;
		o.locked_ = false;
	}

	unique_lock&operator=(unique_lock&&o)
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

	void swap(unique_lock&o)
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

	interruptible_lock(const interruptible_lock &) = delete;
	interruptible_lock &operator=(const interruptible_lock &) = delete;

private:
	T &m_;
	bool locked_;
};

#endif /* __cplusplus */

#endif /* sync_h */
