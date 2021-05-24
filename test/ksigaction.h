#pragma once

#include <cassert>
#include <types.h>

/* copied from musl */
struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)();
	k_sigset_t mask;
};

static_assert(sizeof(k_sigset_t) == sizeof(unsigned) * 2);
