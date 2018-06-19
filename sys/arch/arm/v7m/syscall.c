#include "syscall.h"

#include <access.h>
#include <arch.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <syscall.h>
#include <thread.h>

/*
 * architecture specific syscalls for arm v7m.
 */
__attribute__((used)) int
arch_syscall(long sc, long a0)
{
	switch (sc) {
	case __ARM_NR_set_tls:
		if (!u_address((void*)a0))
			return DERR(-EFAULT);
		context_set_tls(&thread_cur()->ctx, (void*)a0);
		return 0;
	}
	dbg("WARNING: unimplemented syscall %ld\n", sc);
	return DERR(-ENOSYS);
}

/*
 * vfork requires extended syscall frame
 */
__attribute__((naked)) void
sc_vfork(void)
{
	asm(
		"movw r0, :lower16:sc_vfork_\n"
		"movt r0, :upper16:sc_vfork_\n"
		"b asm_extended_syscall\n"
	);
}

/*
 * clone requires extended syscall frame
 */
__attribute__((naked)) void
sc_clone(unsigned long flags, void *sp, void *ptid, unsigned long tls,
    void *ctid)
{
	asm(
		"movw r0, :lower16:sc_clone_\n"
		"movt r0, :upper16:sc_clone_\n"
		"b asm_extended_syscall\n"
	);
}
