#include <arch/interrupt.h>

#include <cpu.h>

void
interrupt_enable()
{
	asm volatile("wrteei 1" ::: "memory");
}

void
interrupt_disable()
{
	asm volatile("wrteei 0" ::: "memory");
}

void
interrupt_save(int *l)
{
	*l = mfmsr().r;
}

void
interrupt_restore(int l)
{
	asm volatile("wrtee %0" :: "r"(l) : "memory");
}

bool
interrupt_enabled()
{
	return mfmsr().EE;
}
