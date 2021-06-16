#pragma once

#include <array>
#include <lib/expect.h>
#include <span>

struct as;

/*
 * elf_load - load elf image into address space from kernel file handle fd.
 *
 * Returns entry point, stack pointer and auxiliary vector on success.
 */
struct elf_load_result {
	void (*entry)();
	void *sp;
	std::array<unsigned, 24> auxv;
};
expect<elf_load_result>
elf_load(as *, int fd);

/*
 * build_args - build arguments onto stack.
 *
 * returns stack pointer ready for new thread.
 */
expect<void *>
build_args(as *, void *stack, const char *const prgv[],
	   const char *const argv[], const char *const envp[],
	   std::span<const unsigned> auxv);

