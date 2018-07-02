#include <arch.h>

#include <debug.h>
#include <sys/auxv.h>

void
arch_backtrace(struct thread *th)
{
	/* TODO: backtrace support for ARM. Requires unwind tables */
}

bool
arch_check_elfhdr(const Elf32_Ehdr *h)
{
	/* must be ARM */
	if (h->e_machine != EM_ARM)
		return false;
	/* must be thumb */
	if (!(h->e_entry & 1))
		return false;
	/* must be version 5 EABI */
	if (EF_ARM_EABI_VERSION(h->e_flags) != EF_ARM_EABI_VER5)
		return false;
	return true;
}

unsigned
arch_elf_hwcap(void)
{
	/* TODO: incomplete */
	return HWCAP_TLS;   /* emulated */
}

void *
arch_stack_align(void *sp)
{
	/* userspace expects 16-byte aligned stack */
	return (void*)((intptr_t)sp & -16);
}

