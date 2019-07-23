#include <sync.h>

#include <arch.h>
#include <irq.h>
#include <sch.h>
#include <thread.h>

#if defined(CONFIG_SMP)

#error not yet implemented

#else

inline void
spinlock_init(struct spinlock *s)
{
#if defined(CONFIG_DEBUG)
	s->owner = 0;
#endif
}

inline void
spinlock_lock(struct spinlock *s)
{
	sch_lock();
#if defined(CONFIG_DEBUG)
	assert(!s->owner);
	assert(!interrupt_running());
	s->owner = thread_cur();
	++thread_cur()->spinlock_locks;
#endif
}

inline void
spinlock_unlock(struct spinlock *s)
{
#if defined(CONFIG_DEBUG)
	assert(s->owner == thread_cur());
	assert(!interrupt_running());
	s->owner = 0;
	--thread_cur()->spinlock_locks;
#endif
	sch_unlock();
}

inline int
spinlock_lock_irq_disable(struct spinlock *s)
{
	const int i = irq_disable();
#if defined(CONFIG_DEBUG)
	assert(!s->owner);
	s->owner = thread_cur();
	++thread_cur()->spinlock_locks;
#endif
	return i;
}

inline void
spinlock_unlock_irq_restore(struct spinlock *s, int v)
{
#if defined(CONFIG_DEBUG)
	s->owner = 0;
	--thread_cur()->spinlock_locks;
#endif
	irq_restore(v);
}

inline void
spinlock_assert_locked(const struct spinlock *s)
{
#if defined(CONFIG_DEBUG)
	assert(s->owner == thread_cur());
#endif
}

#endif
