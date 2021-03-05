#include <sync.h>

#include <arch/interrupt.h>
#include <irq.h>
#include <sch.h>
#include <thread.h>

#if defined(CONFIG_SMP)

#error not yet implemented

#else

void
spinlock_init(spinlock *s)
{
#if defined(CONFIG_DEBUG)
	s->owner = 0;
#endif
}

void
spinlock_lock(spinlock *s)
{
	sch_lock();
#if defined(CONFIG_DEBUG)
	assert(!s->owner);
	assert(!interrupt_running());
	s->owner = thread_cur();
	++thread_cur()->spinlock_locks;
#endif
}

void
spinlock_unlock(spinlock *s)
{
#if defined(CONFIG_DEBUG)
	assert(s->owner == thread_cur());
	assert(!interrupt_running());
	s->owner = 0;
	--thread_cur()->spinlock_locks;
#endif
	sch_unlock();
}

int
spinlock_lock_irq_disable(spinlock *s)
{
	const int i = irq_disable();
#if defined(CONFIG_DEBUG)
	assert(!s->owner);
	s->owner = thread_cur();
	++thread_cur()->spinlock_locks;
#endif
	return i;
}

void
spinlock_unlock_irq_restore(spinlock *s, int v)
{
#if defined(CONFIG_DEBUG)
	s->owner = 0;
	--thread_cur()->spinlock_locks;
#endif
	irq_restore(v);
}

void
spinlock_assert_locked(const spinlock *s)
{
#if defined(CONFIG_DEBUG)
	assert(s->owner == thread_cur());
#endif
}

#endif
