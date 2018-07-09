#include <boot.h>

#include <elf.h>
#include <string.h>
#include <sys/include/arch.h>
#include <sys/include/kernel.h>

#define edbg(...)

static int
load_executable(const phys *img)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;

	ehdr = (Elf32_Ehdr *)img;
	phdr = (Elf32_Phdr *)(img + ehdr->e_phoff);

	for (int i = 0; i < (int)ehdr->e_phnum; i++, phdr++) {
		edbg("\n[PHDR %d]\n", i);
		edbg("p_type=%x\n", phdr->p_type);
		edbg("p_offset=%x\n", phdr->p_offset);
		edbg("p_vaddr=%x\n", phdr->p_vaddr);
		edbg("p_paddr=%x\n", phdr->p_paddr);
		edbg("p_filesz=%x\n", phdr->p_filesz);
		edbg("p_memsz=%x\n", phdr->p_memsz);
		edbg("p_flags=%x\n", phdr->p_flags);
		edbg("p_align=%x\n", phdr->p_align);
		edbg("\n");

		if (phdr->p_type != PT_LOAD) {
			edbg("not PT_LOAD, skip\n");
			continue;
		}

		if (phdr->p_flags & PF_X &&
		    phys_to_virt(img + phdr->p_offset) == (void*)phdr->p_vaddr) {
			edbg("xip\n");
			continue;
		}

		if (phdr->p_filesz > 0) {
			edbg("load: addr=%p from=%p size=%zu\n",
			    virt_to_phys((void*)phdr->p_vaddr),
			    img + phdr->p_offset,
			    (size_t)phdr->p_filesz);
			memcpy(virt_to_phys((void*)phdr->p_vaddr),
			    img + phdr->p_offset,
			    (size_t)phdr->p_filesz);
		}

		if (phdr->p_memsz > phdr->p_filesz) {
			char *offset = (char *)phdr->p_vaddr + phdr->p_filesz;
			size_t size = phdr->p_memsz - phdr->p_filesz;
			edbg("zero: addr=%p size=%zu\n", virt_to_phys(offset),
			    size);
			memset((void *)virt_to_phys(offset), 0, size);
		}

		/* reserve memory */
		bootinfo->ram[bootinfo->nr_rams++] = (struct boot_mem){
			.base = virt_to_phys((void*)phdr->p_vaddr),
			.size = phdr->p_memsz,
			.type = MT_KERNEL,
		};
	}

	edbg("\n");

	kernel_entry = (void*)virt_to_phys((void*)ehdr->e_entry);
	return 0;
}

/*
 * Load the program from specified ELF image data stored in memory.
 * The boot information is filled after loading the program.
 */
int
load_elf(const phys *img)
{
	const Elf32_Ehdr *ehdr = (Elf32_Ehdr *)img;

	/*  Check ELF header */
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr->e_ident[EI_MAG3] != ELFMAG3) {
		dbg("Bad ELF header\n");
		return -1;
	}

	switch (ehdr->e_type) {
	case ET_EXEC:
		if (load_executable(img) != 0)
			return -1;
		break;
	default:
		edbg("Unsupported file type\n");
		return -1;
	}

	return 0;
}
