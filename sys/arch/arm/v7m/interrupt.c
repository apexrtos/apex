#include <arch.h>

#include <assert.h>
#include <cpu.h>
#include <thread.h>

void
interrupt_enable(void)
{
	asm volatile("cpsie i" ::: "memory");
}

void
interrupt_disable(void)
{
	asm volatile("cpsid i" ::: "memory");
}

void
interrupt_save(int *l)
{
	asm volatile("mrs %0, primask" : "=r" (*l));
}

void
interrupt_restore(int l)
{
	asm volatile("msr primask, %0" :: "r" (l) : "memory");
}

void
interrupt_mask(int vector)
{
	assert(vector <= 495);

	/* disable */
	NVIC->ICER[vector / 32] = 1 << vector % 32;
}

void
interrupt_unmask(int vector, int level)
{
	assert(vector <= 495);

	/* set priority */
	NVIC->IPR[vector] = level;

	/* enable */
	NVIC->ISER[vector / 32] = 1 << vector % 32;
}

void
interrupt_setup(int vector, int mode)
{
	/* nothing to do */
}

void
interrupt_init(void)
{
	/* nothing to do */
}

int
interrupt_to_ist_priority(int prio)
{
	/* Map interrupt priority IPL_* to a thread priority between
	   PRI_IST_MAX (16) and PRI_IST_MIN (32) */
	assert(prio < 256);
	return PRI_IST_MAX + prio / 16;
}

bool
interrupt_from_userspace(void)
{
	/* if we are the only active exception we must have come from userspace */
	return SCB->ICSR.RETTOBASE;
}
