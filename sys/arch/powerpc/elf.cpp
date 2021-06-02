#include <arch/elf.h>

#include <cassert>

bool
arch_check_elfhdr(const Elf32_Ehdr *h)
{
	if (h->e_machine != EM_PPC)
		return false;
	/* REVISIT: any other checks required? */
	return true;
}

unsigned
arch_elf_hwcap()
{
	/* REVISIT: do we need to set any flags here? */
	return 0;
}
