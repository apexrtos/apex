/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * elf.c - ELF file format support
 */

#include <boot.h>
#include <sys/elf.h>

#define SHF_VALID	(SHF_ALLOC | SHF_EXECINSTR | SHF_ALLOC | SHF_WRITE)

static char *sect_addr[32];	/* array of section address */

static int
load_executable(char *img, struct module *m)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	paddr_t phys_base;
	int i;

	phys_base = load_base;
	ehdr = (Elf32_Ehdr *)img;
	phdr = (Elf32_Phdr *)((paddr_t)ehdr + ehdr->e_phoff);
	m->phys = load_base;
	phys_base = load_base;
	ELFDBG(("phys addr=%x\n", phys_base));

	for (i = 0; i < (int)ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type != PT_LOAD)
			continue;

		ELFDBG(("p_flags=%x\n", (int)phdr->p_flags));
		ELFDBG(("p_align=%x\n", (int)phdr->p_align));
		ELFDBG(("p_paddr=%x\n", (int)phdr->p_paddr));

		if (i >= 2) {
			ELFDBG(("skipping extra phdr\n"));
			continue;
		}
		if (phdr->p_flags & PF_X) {
			/* Text */
			m->text = phdr->p_vaddr;
			m->textsz = (size_t)phdr->p_memsz;
		} else {
			/* Data & BSS */
			m->data = phdr->p_vaddr;
			m->datasz = (size_t)phdr->p_filesz;
			m->bsssz =
				(size_t)(phdr->p_memsz - phdr->p_filesz);
			load_base = phys_base + (m->data - m->text);
		}
		if (phdr->p_filesz > 0) {
			memcpy((char *)load_base, img + phdr->p_offset,
			       (size_t)phdr->p_filesz);
			ELFDBG(("load: offset=%x size=%x\n",
				 load_base, (int)phdr->p_filesz));
		}
		if (!(phdr->p_flags & PF_X)) {
			if (m->bsssz > 0) {
				/* Zero fill BSS */
				memset((char *)load_base + m->datasz,
				       0, m->bsssz);
			}
			load_base += phdr->p_memsz;
		}
	}
	/* workaround for data/bss size is 0 */
	if (m->data == 0)
		load_base = phys_base + m->textsz;

	load_base = PAGE_ALIGN(load_base);
	m->size = (size_t)(load_base - m->phys);
	m->entry = ehdr->e_entry;
	ELFDBG(("module size=%x entry=%x\n", m->size, m->entry));
	return 0;
}

static int
relocate_section_rela(Elf32_Sym *sym_table, Elf32_Rela *rela,
		      char *target_sect, int nr_reloc)
{
	Elf32_Sym *sym;
	Elf32_Addr sym_val;
	int i;

	for (i = 0; i < nr_reloc; i++) {
		sym = &sym_table[ELF32_R_SYM(rela->r_info)];
		if (sym->st_shndx != STN_UNDEF) {
			sym_val = (Elf32_Addr)sect_addr[sym->st_shndx]
				+ sym->st_value;
			if (relocate_rela(rela, sym_val, target_sect) != 0)
				return -1;
		} else if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
			ELFDBG(("Undefined symbol for rela[%x] sym=%x\n",
				 i, sym));
			return -1;
		} else {
			ELFDBG(("Undefined weak symbol for rela[%x]\n", i));
		}
		rela++;
	}
	return 0;
}

static int
relocate_section_rel(Elf32_Sym *sym_table, Elf32_Rel *rel,
		     char *target_sect, int nr_reloc)
{
	Elf32_Sym *sym;
	Elf32_Addr sym_val;
	int i;

	for (i = 0; i < nr_reloc; i++) {
		sym = &sym_table[ELF32_R_SYM(rel->r_info)];
		if (sym->st_shndx != STN_UNDEF) {
			sym_val = (Elf32_Addr)sect_addr[sym->st_shndx]
				+ sym->st_value;
			if (relocate_rel(rel, sym_val, target_sect) != 0)
				return -1;
		} else if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
			ELFDBG(("Undefined symbol for rel[%x] sym=%x\n",
				 i, sym));
			return -1;
		} else {
			ELFDBG(("Undefined weak symbol for rel[%x]\n", i));
		}
		rel++;
	}
	return 0;
}

static int
relocate_section(char *img, Elf32_Shdr *shdr)
{
	Elf32_Sym *sym_table;
	char *target_sect;
	int nr_reloc, err;

	if (shdr->sh_entsize == 0)
		return 0;
	if ((target_sect = sect_addr[shdr->sh_info]) == 0)
		return -1;
	if ((sym_table = (Elf32_Sym *)sect_addr[shdr->sh_link]) == 0)
		return -1;

	nr_reloc = (int)(shdr->sh_size / shdr->sh_entsize);
	switch (shdr->sh_type) {
	case SHT_REL:
		err = relocate_section_rel(sym_table,
				(Elf32_Rel *)(img + shdr->sh_offset),
				target_sect, nr_reloc);
		break;

	case SHT_RELA:
		err = relocate_section_rela(sym_table,
				(Elf32_Rela *)(img + shdr->sh_offset),
				target_sect, nr_reloc);
		break;

	default:
		err = -1;
		break;
	}
	return err;
}

static int
load_relocatable(char *img, struct module *m)
{
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr;
	paddr_t sect_base, bss_base;
	int i;

	ehdr = (Elf32_Ehdr *)img;
	shdr = (Elf32_Shdr *)((paddr_t)ehdr + ehdr->e_shoff);
	bss_base = 0;
	m->phys = load_base;
	ELFDBG(("phys addr=%x\n", load_base));

	/* Copy sections */
	for (i = 0; i < (int)ehdr->e_shnum; i++, shdr++) {
		sect_addr[i] = 0;
		if (shdr->sh_type == SHT_PROGBITS) {

			ELFDBG(("sh_addr=%x\n", shdr->sh_addr));
			ELFDBG(("sh_size=%x\n", shdr->sh_size));
			ELFDBG(("sh_offset=%x\n", shdr->sh_offset));
			ELFDBG(("sh_flags=%x\n", shdr->sh_flags));

			switch (shdr->sh_flags & SHF_VALID) {
			case (SHF_ALLOC | SHF_EXECINSTR):
				/* Text */
				m->text = (vaddr_t)phys_to_virt(load_base);
				break;
			case (SHF_ALLOC | SHF_WRITE):
				/* Data */
				if (m->data == 0)
					m->data =
						(vaddr_t)phys_to_virt(load_base + shdr->sh_addr);
				break;
			case SHF_ALLOC:
				/* rodata */
				/* Note: rodata is treated as text. */
				break;
			default:
				continue;
			}
			sect_base = load_base + shdr->sh_addr;
			memcpy((char *)sect_base, img + shdr->sh_offset,
			       (size_t)shdr->sh_size);
			ELFDBG(("load: offset=%x size=%x\n",
				 sect_base, (int)shdr->sh_size));

			sect_addr[i] = (char *)sect_base;
		} else if (shdr->sh_type == SHT_NOBITS) {
			/* BSS */
			m->bsssz = (size_t)shdr->sh_size;
			sect_base = load_base + shdr->sh_addr;
			bss_base = sect_base;

			/* Zero fill BSS */
			memset((char *)sect_base, 0, (size_t)shdr->sh_size);

			sect_addr[i] = (char *)sect_base;
		} else if (shdr->sh_type == SHT_SYMTAB) {
			/* Symbol table */
			sect_addr[i] = img + shdr->sh_offset;
		}
	}
	m->textsz = (size_t)(m->data - m->text);
	m->datasz = (size_t)((size_t)phys_to_virt(bss_base) - m->data);

	load_base = bss_base + m->bsssz;
	load_base = PAGE_ALIGN(load_base);

	ELFDBG(("module load_base=%x text=%x\n", load_base, m->text));
	m->size = (size_t)(load_base - (paddr_t)virt_to_phys(m->text));
	m->entry = (vaddr_t)phys_to_virt(ehdr->e_entry + m->phys);
	ELFDBG(("module size=%x entry=%x\n", m->size, m->entry));

	/* Process relocation */
	shdr = (Elf32_Shdr *)((paddr_t)ehdr + ehdr->e_shoff);
	for (i = 0; i < (int)ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) {
			if (relocate_section(img, shdr) != 0)
				return -1;
		}
	}
	return 0;
}

/*
 * Load the program from specified ELF image data stored in memory.
 * The boot information is filled after loading the program.
 */
int
elf_load(char *img, struct module *m)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;

	ELFDBG(("\nelf_load\n"));

	ehdr = (Elf32_Ehdr *)img;

	/*  Check ELF header */
	if ((ehdr->e_ident[EI_MAG0] != ELFMAG0) ||
	    (ehdr->e_ident[EI_MAG1] != ELFMAG1) ||
	    (ehdr->e_ident[EI_MAG2] != ELFMAG2) ||
	    (ehdr->e_ident[EI_MAG3] != ELFMAG3))
		return -1;

	phdr = (Elf32_Phdr *)((paddr_t)ehdr + ehdr->e_ehsize);

	if (nr_img == 0) {
		/*  Initialize the load address */
		load_base = (vaddr_t)phys_to_virt(phdr->p_paddr);
		if (load_base == 0)
			return -1;
		ELFDBG(("kernel base=%x\n", load_base));
		load_start = load_base;
	}
	else if (nr_img == 1) {
		/* Second image => Driver */
#if (DRIVER_BASE != AUTODETECT)
		/* Adjust for static driver */
		load_base = (vaddr_t)phys_to_virt(phdr->p_paddr);
#endif
		ELFDBG(("driver base=%x\n", load_base));
	}
	else {
		/* Other images => Boot tasks */
		ELFDBG(("task base=%x\n", load_base));
	}

	switch (ehdr->e_type) {
	case ET_EXEC:
		if (load_executable(img, m) != 0)
			return -1;
		break;
	case ET_REL:
		if (load_relocatable(img, m) != 0)
			return -1;
		break;
	default:
		ELFDBG(("Unsupported file type\n"));
		return -1;
	}
	nr_img++;
	return 0;
}
