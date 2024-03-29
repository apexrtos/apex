/*
 * elf_load.cpp - elf file management
 */
#include <elf_load.h>

#include <arch/elf.h>
#include <arch/stack.h>
#include <debug.h>
#include <elf_native.h>
#include <errno.h>
#include <kernel.h>
#include <mmap.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vm.h>

namespace {

int
ph_flags_to_prot(const ElfN_Phdr &ph)
{
	return
	    (ph.p_flags & PF_R ? PROT_READ : 0) |
	    (ph.p_flags & PF_W ? PROT_WRITE : 0) |
	    (ph.p_flags & PF_X ? PROT_EXEC : 0);
}

}

/*
 * elf_load - load an elf file, attempt to execute in place.
 */
expect<elf_load_result>
elf_load(as *a, int fd)
{
	ElfN_Ehdr eh;

	/* read and validate file header */
	if (auto r{pread(fd, &eh, sizeof eh, 0)}; r != sizeof eh)
		return to_errc(r, DERR(std::errc::executable_format_error));
	if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
	    eh.e_ident[EI_MAG1] != ELFMAG1 ||
	    eh.e_ident[EI_MAG2] != ELFMAG2 ||
	    eh.e_ident[EI_MAG3] != ELFMAG3 ||
	    eh.e_ident[EI_CLASS] != ELFCLASSN ||
	    (eh.e_type != ET_EXEC && eh.e_type != ET_DYN) ||
	    eh.e_phentsize != sizeof(ElfN_Phdr) ||
	    eh.e_phnum < 1 ||
	    !arch_check_elfhdr(&eh))
		return DERR(std::errc::executable_format_error);

	/* determine extent of program image & stack size */
	size_t stack_size = PAGE_SIZE;
	int stack_prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	ElfN_Addr img_beg{std::numeric_limits<ElfN_Addr>::max()};
	ElfN_Addr img_end{std::numeric_limits<ElfN_Addr>::min()};
	for (auto i{0}; i < eh.e_phnum; ++i) {
		ElfN_Phdr ph;

		if (auto r{pread(fd, &ph, sizeof(ph),
				 eh.e_phoff + (i * sizeof(ph)))};
		    r != sizeof ph)
			return to_errc(r, DERR(std::errc::executable_format_error));

		if (ph.p_type == PT_GNU_STACK) {
			stack_size = PAGE_ALIGN(ph.p_memsz);
			stack_prot = ph_flags_to_prot(ph);
			continue;
		}

		if (ph.p_type != PT_LOAD || !ph.p_memsz)
			continue;

		if (ph.p_filesz > ph.p_memsz)
			return DERR(std::errc::executable_format_error);

		if (ph.p_align < PAGE_SIZE)
			return DERR(std::errc::executable_format_error);

		img_beg = std::min(img_beg, ph.p_vaddr);
		img_end = std::max(img_end, ph.p_vaddr + ph.p_memsz);
	}

	const auto dyn{eh.e_type == ET_DYN};
	int flags{MAP_PRIVATE | (dyn ? 0 : MAP_FIXED)};
	std::byte *load;

	/* dynamic executables should start at address 0 */
	if (dyn && img_beg)
		return DERR(std::errc::executable_format_error);

#if defined(CONFIG_MMU)
	load = dyn ? random_load_address() : img_beg;
#else
	/* create a mapping covering the entire program image */
	if (auto r = mmapfor(a, (void*)img_beg, img_end - img_beg, PROT_NONE,
			     flags | MAP_ANONYMOUS, -1, 0, MA_NORMAL);
	    !r.ok())
		return r.err();
	else load = (std::byte *)r.val();
#endif

	flags |= MAP_FIXED;
	auto base{dyn ? load : 0};

	/* load program segments */
	for (auto i{0}; i < eh.e_phnum; ++i) {
		ElfN_Phdr ph;

		if (auto r{pread(fd, &ph, sizeof(ph),
				 eh.e_phoff + (i * sizeof(ph)))};
		    r != sizeof ph)
			return to_errc(r, DERR(std::errc::executable_format_error));

		if (ph.p_type != PT_LOAD || !ph.p_memsz)
			continue;
		auto vaddr{base + ph.p_vaddr};

		if (ph.p_filesz)
			if (auto r{mmapfor(a, vaddr, ph.p_filesz,
					   ph_flags_to_prot(ph), flags, fd,
					   ph.p_offset, MA_NORMAL)};
			    !r.ok())
				return r.err();

		const auto file_end{PAGE_ALIGN(vaddr + ph.p_filesz)};
		const auto mem_end{vaddr + ph.p_memsz};
		if (mem_end > file_end)
			if (auto r{mmapfor(a, file_end,
					   mem_end - file_end,
					   ph_flags_to_prot(ph),
					   flags | MAP_ANONYMOUS, -1, 0,
					   MA_NORMAL)};
			    !r.ok())
				return r.err();
	}

#if !defined(CONFIG_MMU)
	/* REVISIT: unmap holes in the program image? */
#endif

	/* REVISIT: assuming the data segment is last.. */
	vm_init_brk(a, base + PAGE_ALIGN(img_end));

	elf_load_result res;

	/* map stack with optional guard page */
#if defined(CONFIG_MMU) || defined(CONFIG_MPU)
	const size_t guard_size = PAGE_SIZE;
#else
	const size_t guard_size = 0;
#endif
	if (auto r = mmapfor(a, 0, stack_size + guard_size, PROT_NONE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, MA_NORMAL);
	    !r.ok())
		return r.err();
	else res.sp = r.val();

	if (auto r = mprotectfor(a, (std::byte*)res.sp + guard_size,
				 stack_size, stack_prot);
	    !r.ok())
		return r.err();

	res.sp = (std::byte*)res.sp + stack_size + guard_size;
	res.entry = (void(*)())(base + eh.e_entry);

	/*
	 * Populate auxv
	 */
	res.auxv[0] = AT_PHDR;	    res.auxv[1] = (uintptr_t)(load + eh.e_phoff);
	res.auxv[2] = AT_PHENT;	    res.auxv[3] = sizeof(ElfN_Phdr);
	res.auxv[4] = AT_PHNUM;	    res.auxv[5] = eh.e_phnum;
	res.auxv[6] = AT_PAGESZ;    res.auxv[7] = PAGE_SIZE;
	res.auxv[8] = AT_BASE;	    res.auxv[9] = (uintptr_t)load;
	res.auxv[10] = AT_ENTRY;    res.auxv[11] = (uintptr_t)res.entry;
	res.auxv[12] = AT_UID;	    res.auxv[13] = 500;
	res.auxv[14] = AT_EUID;	    res.auxv[15] = 500;
	res.auxv[16] = AT_GID;	    res.auxv[17] = 500;
	res.auxv[18] = AT_EGID;	    res.auxv[19] = 500;
	res.auxv[20] = AT_HWCAP;    res.auxv[21] = arch_elf_hwcap();
	res.auxv[22] = AT_NULL;	    res.auxv[23] = 0; /* terminating entry */

	return res;
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
expect<void *>
build_args(as *a, void *stack, const char *const prgv[],
	   const char *const argv[], const char *const envp[],
	   std::span<const unsigned> auxv)
{
	const ssize_t argsz{sizeof(void*)}, zero{0};
	ssize_t i, argc, auxvlen, strtot{0}, argtot{1};

	assert(stack && argv);

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
	char *str = (char*)stack - strtot;
	char *arg = (char*)arch_ustack_align(str - argtot * argsz);
	void *const sp = arg;

	/* Copy in strings & fill in arguments */
	if (vm_write(a, &argc, arg, argsz) != argsz)
		return DERR(std::errc::not_enough_memory);
	arg += argsz;
	for (i = 0; prgv && prgv[i]; ++i) {
		if (vm_write(a, &str, arg, argsz) != argsz)
			return DERR(std::errc::not_enough_memory);
		arg += argsz;
		const ssize_t len = strlen(prgv[i]) + 1;
		if (vm_write(a, prgv[i], str, len) != len)
			return DERR(std::errc::not_enough_memory);
		str += len;
	}
	for (i = 0; argv[i]; ++i) {
		if (vm_write(a, &str, arg, argsz) != argsz)
			return DERR(std::errc::not_enough_memory);
		arg += argsz;
		const ssize_t len = strlen(argv[i]) + 1;
		if (vm_write(a, argv[i], str, len) != len)
			return DERR(std::errc::not_enough_memory);
		str += len;
	}
	if (vm_write(a, &zero, arg, argsz) != argsz)
		return DERR(std::errc::not_enough_memory);
	arg += argsz;
	for (i = 0; envp && envp[i]; ++i) {
		if (vm_write(a, &str, arg, argsz) != argsz)
			return DERR(std::errc::not_enough_memory);
		arg += argsz;
		const ssize_t len = strlen(envp[i]) + 1;
		if (vm_write(a, envp[i], str, len) != len)
			return DERR(std::errc::not_enough_memory);
		str += len;
	}
	if (vm_write(a, &zero, arg, argsz) != argsz)
		return DERR(std::errc::not_enough_memory);
	arg += argsz;
	if (vm_write(a, data(auxv), arg, auxvlen * argsz) != auxvlen * argsz)
		return DERR(std::errc::not_enough_memory);

	return sp;
}

