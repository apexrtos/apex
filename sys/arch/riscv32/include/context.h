#pragma once

#include <cstdint>

/*
 * Context stored in struct thread
 */
struct context {
	uintptr_t ksp;			/* kernel stack pointer */
	uintptr_t kstack;		/* top of kernel stack */
	uintptr_t xsp;			/* stack pointer at trap entry */
	unsigned long irq_nesting;	/* irq nesting counter */
};
