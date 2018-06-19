#ifndef elf_load_h
#define elf_load_h

struct as;
#define AUX_CNT 24

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * elf_load - load elf image into mmu map from kernel file handle fd.
 *
 * *entry is set to the entry point of the elf image on success.
 */
int elf_load(struct as *, int fd, void (**entry)(void),
	     unsigned auxv[AUX_CNT]);

/*
 * build_args - build arguments onto stack.
 *
 * stack is assumed to be of USTACK_SIZE.
 *
 * returns stack pointer ready for new thread.
 */
void *build_args(struct as *, void *stack, const char *const prgv[],
		 const char *const argv[], const char *const envp[],
		 const unsigned auxv[]);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* !elf_load_h */

