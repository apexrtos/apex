#include <arch/stack.h>

#include <cstdint>

void *
arch_ustack_align(void *sp)
{
	/* 16-byte aligned stack */
	return (void*)((uintptr_t)sp & -16);
}

void *
arch_kstack_align(void *sp)
{
	/* 16-byte aligned stack */
	return (void*)((uintptr_t)sp & -16);
}

