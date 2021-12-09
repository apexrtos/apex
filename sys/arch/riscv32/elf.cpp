#include <arch/elf.h>

#include <cassert>

bool
arch_check_elfhdr(const Elf32_Ehdr *h)
{
	if (h->e_machine != EM_RISCV)
		return false;
	/* REVISIT: anything else to check here? */
	return true;
}

unsigned
arch_elf_hwcap()
{
	return 0;
}
