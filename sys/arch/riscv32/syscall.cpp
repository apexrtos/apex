#include "locore.h"

#include <debug.h>
#include <errno.h>

long
arch_syscall(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
	     uint32_t a5, uint32_t a6, uint32_t a7)
{
	dbg("WARNING: unimplemented syscall %u\n", a7);
	return DERR(-ENOSYS);
}
