#include <sync.h>

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
	s->owner = thread_cur();
#endif
}

inline void
spinlock_unlock(struct spinlock *s)
{
#if defined(CONFIG_DEBUG)
	assert(s->owner == thread_cur());
	s->owner = 0;
#endif
	sch_unlock();
}

inline int
spinlock_lock_irq_disable(struct spinlock *s)
{
	sch_lock();
	const int i = irq_disable();
#if defined(CONFIG_DEBUG)
	assert(!s->owner);
	s->owner = thread_cur();
#endif
	return i;
}

inline void
spinlock_unlock_irq_restore(struct spinlock *s, int v)
{
#if defined(CONFIG_DEBUG)
	s->owner = 0;
#endif
	irq_restore(v);
	sch_unlock();
}

inline void
spinlock_assert_locked(struct spinlock *s)
{
#if defined(CONFIG_DEBUG)
	assert(s->owner == thread_cur());
#endif
}

#endif
