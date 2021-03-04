#pragma once

/*
 * arch/elf.h - architecture specific elf management
 */

#include <elf.h>

bool arch_check_elfhdr(const Elf32_Ehdr *);
unsigned arch_elf_hwcap();
