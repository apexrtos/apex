/*
 * elf.c - elf file management
 */
#include <elf_load.h>

#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <elf.h>
#include <errno.h>
#include <fs.h>
#include <kernel.h>
#include <mmap.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vm.h>

#if UINTPTR_MAX == 0xffffffff
typedef Elf32_Ehdr Ehdr;
typedef Elf32_Phdr Phdr;
#else
typedef Elf64_Ehdr Ehdr;
typedef Elf64_Phdr Phdr;
#endif

static int
ph_flags_to_prot(const Phdr *ph)
{
	return
	    (ph->p_flags & PF_R ? PROT_READ : 0) |
	    (ph->p_flags & PF_W ? PROT_WRITE : 0) |
	    (ph->p_flags & PF_X ? PROT_EXEC : 0);
}

/*
 * elf_load - load an elf file, attempt to execute in place.
 */
int
elf_load(struct as *a, int fd, void (**entry)(void), unsigned auxv[AUX_CNT],
    void **stack)
{
	int err;
	Ehdr eh;
	ssize_t sz;
	Phdr text_ph, data_ph, stack_ph;

	text_ph.p_type = PT_NULL;
	data_ph.p_type = PT_NULL;
	stack_ph.p_type = PT_NULL;

	/*
	 * Read and validate file header
	 */
	if ((sz = pread(fd, &eh, sizeof(eh), 0)) < 0)
		return DERR(sz);

	if (sz != sizeof(eh) ||
	    eh.e_ident[EI_MAG0] != ELFMAG0 ||
	    eh.e_ident[EI_MAG1] != ELFMAG1 ||
	    eh.e_ident[EI_MAG2] != ELFMAG2 ||
	    eh.e_ident[EI_MAG3] != ELFMAG3 ||
	    (eh.e_type != ET_EXEC && eh.e_type != ET_DYN) ||
	    eh.e_phentsize != sizeof(Phdr) ||
	    eh.e_phnum < 1 ||
	    !arch_check_elfhdr(&eh))
		return DERR(-ENOEXEC);

	/*
	 * Process program headers
	 */
	for (size_t i = 0; i < eh.e_phnum; ++i) {
		Phdr ph;

		if ((sz = pread(fd, &ph, sizeof(ph),
		    eh.e_phoff + (i * sizeof(ph)))) < 0)
			return DERR(sz);
		if (sz != sizeof(ph))
			return DERR(-ENOEXEC);

		/* TODO: support dynamic linking */
		if (ph.p_type == PT_INTERP)
			return DERR(-ENOEXEC);

		if (ph.p_type == PT_GNU_STACK) {
			/* only expect one stack segment */
			if (stack_ph.p_type != PT_NULL)
				return DERR(-ENOEXEC);
			stack_ph = ph;
		}

		if (ph.p_type != PT_LOAD)
			continue;

		if (ph.p_memsz == 0 || ph.p_filesz > ph.p_memsz)
			return DERR(-ENOEXEC);

		if (ph.p_flags & PF_X) {
			/* only expect one text segment */
			if (text_ph.p_type != PT_NULL)
				return DERR(-ENOEXEC);
			text_ph = ph;
		} else {
			/* only expect one data segment */
			if (data_ph.p_type != PT_NULL)
				return DERR(-ENOEXEC);
			data_ph = ph;
		}
	}

	/* expect text, data & stack segment */
	if (text_ph.p_type == PT_NULL || data_ph.p_type == PT_NULL ||
	    stack_ph.p_type == PT_NULL || !stack_ph.p_memsz)
		return DERR(-ENOEXEC);

	const intptr_t text_start = text_ph.p_vaddr; /* must be aligned */
	const intptr_t text_end = PAGE_ALIGN(text_ph.p_vaddr + text_ph.p_memsz);
	const intptr_t data_start = PAGE_TRUNC(data_ph.p_vaddr);
	const intptr_t data_end = PAGE_ALIGN(data_ph.p_vaddr + data_ph.p_memsz);

	/* expect data to be after text */
	if (data_start < text_end)
		return DERR(-ENOEXEC);

	const bool dyn = eh.e_type == ET_DYN;
	const size_t image_sz = data_end - text_start;
	int flags = MAP_PRIVATE | (dyn ? 0 : MAP_FIXED);
	void *base, *text, *data;

	/* create a mapping covering the program image */
	if ((base = mmapfor(a, (void *)text_start, image_sz, PROT_NONE,
	    flags | MAP_ANONYMOUS, -1, 0, MEM_NORMAL)) > (void*)-4096UL)
		return (int)base;

	flags |= MAP_FIXED;

	/* map text */
	if ((text = mmapfor(a, base, text_end - text_start,
	    ph_flags_to_prot(&text_ph), flags, fd, text_ph.p_offset,
	    MEM_NORMAL)) > (void*)-4096UL)
		return (int)text;

	/* offset data if text-to-data offset must be maintained */
	void *const data_vaddr = data_start + (dyn ? base : 0);

	/* map data */
	if ((data = mmapfor(a, data_vaddr, data_end - data_start,
	    ph_flags_to_prot(&data_ph), flags, fd, PAGE_TRUNC(data_ph.p_offset),
	    MEM_NORMAL)) > (void*)-4096UL)
		return (int)data;
	vm_init_brk(a, dyn ? data + data_end : (void*)data_end);

	/* unmap any text-to-data hole */
	if ((err = munmapfor(a, (void *)text_end, data_start - text_end)) < 0)
		return err;

	/* map stack with optional guard page */
	const size_t stack_size = PAGE_ALIGN(stack_ph.p_memsz);
#if defined(CONFIG_MMU) || defined(CONFIG_MPU)
	const size_t guard_size = CONFIG_PAGE_SIZE;
#else
	const size_t guard_size = 0;
#endif
	if ((*stack = mmapfor(a, 0, stack_size + guard_size, PROT_NONE,
	    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, MEM_NORMAL)) > (void*)-4096UL)
		return (int)*stack;
	if ((err = mprotectfor(a, (char*)*stack + guard_size, stack_size,
	    ph_flags_to_prot(&stack_ph))) < 0)
		return err;
	*stack = (char*)*stack + stack_size + guard_size;

	const ptrdiff_t text_load_offset = (ptrdiff_t)base - text_start;

	/*
	 * Populate auxv
	 */
	*auxv++ = AT_PHDR;	*auxv++ = (uintptr_t)(base + eh.e_phoff);
	*auxv++ = AT_PHENT;	*auxv++ = sizeof(Phdr);
	*auxv++ = AT_PHNUM;	*auxv++ = eh.e_phnum;
	*auxv++ = AT_PAGESZ;	*auxv++ = CONFIG_PAGE_SIZE;
	*auxv++ = AT_BASE;	*auxv++ = (uintptr_t)base;
	*auxv++ = AT_ENTRY;	*auxv++ = text_load_offset + eh.e_entry;
	*auxv++ = AT_UID;	*auxv++ = 500;
	*auxv++ = AT_EUID;	*auxv++ = 500;
	*auxv++ = AT_GID;	*auxv++ = 500;
	*auxv++ = AT_EGID;	*auxv++ = 500;
	*auxv++ = AT_HWCAP;	*auxv++ = arch_elf_hwcap();
	*auxv++ = AT_NULL;	*auxv++ = 0;	/* terminating entry */
	static_assert(AUX_CNT >= 24, "");

	*entry = (void*)(text_load_offset + eh.e_entry);
	return 0;
}

/*
 * build_args - build arguments on new stack
 *
 * position            content                     size (bytes)
 * ------------------------------------------------------------------------
 * stack pointer ->  [ argc = number of args ]     4
 *                   [ argv[0] (pointer) ]         4   (program name)
 *                   [ argv[1] (pointer) ]         4
 *                   [ argv[..] (pointer) ]        4 * x
 *                   [ argv[n - 1] (pointer) ]     4
 *                   [ argv[n] (pointer) ]         4   (= NULL)
 *
 *                   [ envp[0] (pointer) ]         4
 *                   [ envp[1] (pointer) ]         4
 *                   [ envp[..] (pointer) ]        4
 *                   [ envp[term] (pointer) ]      4   (= NULL)
 *
 *                   [ auxv[0] (Elf32_auxv_t) ]    8
 *                   [ auxv[1] (Elf32_auxv_t) ]    8
 *                   [ auxv[..] (Elf32_auxv_t) ]   8
 *                   [ auxv[term] (Elf32_auxv_t) ] 8   (= AT_NULL vector)
 *
 *                   [ padding ]                   0 - 3
 *
 *                   [ NULL terminated strings ]   >= 0
 *
 *                   < bottom of stack >           0
 * ------------------------------------------------------------------------
 *
 * returns stack pointer
 */
void *
build_args(struct as *a, void *stack, const char *const prgv[],
    const char *const argv[], const char *const envp[], const unsigned auxv[])
{
	const size_t argsz = sizeof(void *), zero = 0;
	size_t i, argc, auxvlen, strtot = 0, argtot = 1;

	/* No arguments */
	if (!argv)
		return arch_stack_align(stack);

	assert(stack && argv && auxv);

	/* Calculate memory requirements */
	for (i = 0; prgv && prgv[i]; ++i)
		strtot += strlen(prgv[i]) + 1;
	argc = i;
	argtot += i;
	for (i = 0; argv[i]; ++i)
		strtot += strlen(argv[i]) + 1;
	argc += i;
	argtot += i + 1;
	for (i = 0; envp && envp[i]; ++i)
		strtot += strlen(envp[i]) + 1;
	argtot += i + 1;
	for (i = 0; auxv[i]; i += 2);
	auxvlen = i + 2;
	argtot += auxvlen;

	/* Set target stack addresses */
	void *str = stack - strtot;
	void *arg = arch_stack_align(TRUNC(stack - strtot - argtot * argsz));
	void *const sp = arg;

	/* Copy in strings & fill in arguments */
	if (vm_write(a, &argc, arg, argsz) != argsz)
		return (void*)DERR(-ENOMEM);
	arg += argsz;
	for (i = 0; prgv && prgv[i]; ++i) {
		if (vm_write(a, &str, arg, argsz) != argsz)
			return (void*)DERR(-ENOMEM);
		arg += argsz;
		const size_t len = strlen(prgv[i]) + 1;
		if (vm_write(a, prgv[i], str, len) != len)
			return (void*)DERR(-ENOMEM);
		str += len;
	}
	for (i = 0; argv[i]; ++i) {
		if (vm_write(a, &str, arg, argsz) != argsz)
			return (void*)DERR(-ENOMEM);
		arg += argsz;
		const size_t len = strlen(argv[i]) + 1;
		if (vm_write(a, argv[i], str, len) != len)
			return (void*)DERR(-ENOMEM);
		str += len;
	}
	if (vm_write(a, &zero, arg, argsz) != argsz)
		return (void*)DERR(-ENOMEM);
	arg += argsz;
	for (i = 0; envp && envp[i]; ++i) {
		if (vm_write(a, &str, arg, argsz) != argsz)
			return (void*)DERR(-ENOMEM);
		arg += argsz;
		const size_t len = strlen(envp[i]) + 1;
		if (vm_write(a, envp[i], str, len) != len)
			return (void*)DERR(-ENOMEM);
		str += len;
	}
	if (vm_write(a, &zero, arg, argsz) != argsz)
		return (void*)DERR(-ENOMEM);
	arg += argsz;
	if (vm_write(a, auxv, arg, auxvlen * argsz) != auxvlen * argsz)
		return (void*)DERR(-ENOMEM);

	return sp;
}

