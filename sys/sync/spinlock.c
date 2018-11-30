#include <sync.h>

#include <irq.h>
#include <sch.h>

#if defined(CONFIG_SMP)

#error not yet implemented

#else

inline void
spinlock_init(struct spinlock *s)
{

}

inline void
spinlock_lock(struct spinlock *s)
{
	sch_lock();
}

inline void
spinlock_unlock(struct spinlock *s)
{
	sch_unlock();
}

inline int
spinlock_lock_irq_disable(struct spinlock *s)
{
	sch_lock();
	return irq_disable();
}

inline void
spinlock_unlock_irq_restore(struct spinlock *s, int v)
{
	irq_restore(v);
	sch_unlock();
}

#endif
