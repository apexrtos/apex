#pragma once

#include <array>
#include <span>

struct as;
constexpr auto AUX_CNT{24};

/*
 * elf_load - load elf image into mmu map from kernel file handle fd.
 *
 * *entry is set to the entry point of the elf image on success.
 */
int
elf_load(as *, int fd, void (**entry)(void),
	 std::array<unsigned, AUX_CNT> &auxv, void **stack);

/*
 * build_args - build arguments onto stack.
 *
 * returns stack pointer ready for new thread.
 */
void *
build_args(as *, void *stack, const char *const prgv[],
	   const char *const argv[], const char *const envp[],
	   std::span<const unsigned> auxv);

