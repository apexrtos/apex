#include <arch/interrupt.h>

#include <arch/mmio.h>
#include <cassert>
#include <cpu.h>
#include <thread.h>

void
interrupt_enable()
{
	asm volatile("cpsie i" ::: "memory");
}

void
interrupt_disable()
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

bool
interrupt_enabled()
{
	int primask;
	interrupt_save(&primask);
	return primask == 0;
}

void
interrupt_mask(int vector)
{
	assert(vector <= 495);

	/* disable */
	write32(&NVIC->ICER[vector / 32], 1 << vector % 32);
}

void
interrupt_unmask(int vector, int level)
{
	assert(vector <= 495);

	/* set priority */
	write8(&NVIC->IPR[vector], level);

	/* enable */
	write32(&NVIC->ISER[vector / 32], 1 << vector % 32);
}

void
interrupt_setup(int vector, int mode)
{
	/* nothing to do */
}

void
interrupt_init()
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
interrupt_from_userspace()
{
	assert(interrupt_running());

	/* userspace threads are unprivileged */
	int control;
	asm("mrs %0, control" : "=r"(control));
	return control & CONTROL_NPRIV;
}

bool
interrupt_running()
{
	int ipsr;
	asm("mrs %0, ipsr" : "=r" (ipsr));
	return ipsr != 0;
}
