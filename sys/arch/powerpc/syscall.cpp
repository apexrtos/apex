#include <debug.h>
#include <errno.h>

/*
 * architecture specific syscalls for powerpc
 */
extern "C"
int
arch_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long sc)
{
	dbg("WARNING: unimplemented syscall %ld\n", sc);
	return DERR(-ENOSYS);
}
