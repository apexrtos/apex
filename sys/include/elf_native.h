#pragma once

/*
 * elf_native.h - native elf types for current architecture
 */

#include <elf.h>

#if UINTPTR_MAX == 0xffffffff
using ElfN_Ehdr = Elf32_Ehdr;
using ElfN_Phdr = Elf32_Phdr;
using ElfN_Addr = Elf32_Addr;
constexpr auto ELFCLASSN = ELFCLASS32;
#else
using ElfN_Ehdr = Elf64_Ehdr;
using ElfN_Phdr = Elf64_Phdr;
using ElfN_Addr = Elf64_Addr;
constexpr auto ELFCLASSN = ELFCLASS64;
#endif
