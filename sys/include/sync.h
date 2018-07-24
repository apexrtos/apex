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

#include <types.h>

struct list;
struct thread;

#define MUTEX_WAITERS	0x00000001
#define MUTEX_RECURSIVE	0x00000002
#define MUTEX_TID_MASK	0xFFFFFFFC

struct cond {
	union {
		char storage[32];
		unsigned align;
	};
};

struct mutex {
	union {
		char storage[40];
		unsigned align;
	};
};

struct rwlock {
	union {
		char storage[80];
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
int	       mutex_prio(const struct mutex *);
void	       mutex_setprio(struct mutex *, int);
unsigned       mutex_count(const struct mutex *);
struct mutex  *mutex_entry(struct list*);

bool	       cond_valid(const struct cond *);
void	       cond_init(struct cond *);
int	       cond_wait_interruptible(struct cond *, struct mutex *);
int	       cond_timedwait_interruptible(struct cond *, struct mutex *, uint64_t);
int	       cond_signal(struct cond *);
int	       cond_broadcast(struct cond *);

void	       rwlock_init(struct rwlock *);
int	       rwlock_read_lock_interruptible(struct rwlock *);
int	       rwlock_read_unlock(struct rwlock *);
int	       rwlock_write_lock_interruptible(struct rwlock *);
int	       rwlock_write_unlock(struct rwlock *);

#if defined(__cplusplus)
} /* extern "C" */

namespace a {

class mutex {
public:
	mutex() { mutex_init(&m_); }
	int interruptible_lock() { return mutex_lock_interruptible(&m_); }
	int lock() { return mutex_lock(&m_); }
	int unlock() { return mutex_unlock(&m_); }

private:
	::mutex m_;
};

class rwlock {
public:
	rwlock() { rwlock_init(&m_); }
	int interruptible_read_lock() { return rwlock_read_lock_interruptible(&m_); }
	int read_unlock() { return rwlock_read_unlock(&m_); }
	int interruptible_write_lock() { return rwlock_write_lock_interruptible(&m_); }
	int write_unlock() { return rwlock_write_unlock(&m_); }

private:
	::rwlock m_;
};

} /* namespace a */

#endif /* __cplusplus */

#endif /* sync_h */
