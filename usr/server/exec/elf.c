/*
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
 * elf.c - ELF file loader
 */

#include <prex/prex.h>
#include <server/fs.h>
#include <server/proc.h>
#include <sys/elf.h>
#include <sys/param.h>

#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "exec.h"

#define SHF_VALID	(SHF_ALLOC | SHF_EXECINSTR | SHF_ALLOC | SHF_WRITE)

#ifndef CONFIG_MMU
static char *sect_addr[32];	/* Array of section address */
#endif

#ifdef CONFIG_MMU
/*
 * Load executable ELF file
 */
static int
load_exec(Elf32_Ehdr *ehdr, task_t task, int fd, void **entry)
{
	Elf32_Phdr *phdr;
	void *addr, *mapped;
	size_t size = 0;
	int i;

	phdr = (Elf32_Phdr *)((u_long)ehdr + ehdr->e_phoff);
	if (phdr == NULL)
		return ENOEXEC;

	for (i = 0; i < (int)ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type != PT_LOAD)
			continue;

		addr = (void *)phdr->p_vaddr;
		size = phdr->p_memsz;
		if (size == 0)
			continue;

		if (vm_allocate(task, &addr, size, 0) != 0)
			return ENOMEM;

		if (vm_map(task, (void *)phdr->p_vaddr, size, &mapped) != 0)
			return ENOEXEC;
		if (phdr->p_filesz > 0) {
			if (lseek(fd, (off_t)phdr->p_offset, SEEK_SET)
			    == -(off_t)1)
				goto err;
			if (read(fd, mapped, phdr->p_filesz) < 0)
				goto err;
		}

		vm_free(task_self(), mapped);

		/* Set read-only to text */
		if (phdr->p_flags & PF_X) {
			if (vm_attribute(task, addr, VMA_READ) != 0)
				return ENOEXEC;
		}
	}
	*entry = (void *)ehdr->e_entry;
	return 0;

 err:
	vm_free(task_self(), mapped);
	return EIO;
}
#else /* !CONFIG_MMU */

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
		} else if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
			DPRINTF(("undefined weak symbol for rela[%d]\n", i));
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
		} else if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
			DPRINTF(("undefined weak symbol for rel[%d]\n", i));
		}
		rel++;
	}
	return 0;
}

/*
 * Relocate ELF sections
 */
static int
relocate_section(Elf32_Shdr *shdr, char *rel_data)
{
	Elf32_Sym *sym_table;
	char *target_sect;
	int nr_reloc, err;

	DPRINTF(("relocate_sec\n"));
	if (shdr->sh_entsize == 0)
		return 0;
	if ((target_sect = sect_addr[shdr->sh_info]) == 0)
		return -1;
	if ((sym_table = (Elf32_Sym *)sect_addr[shdr->sh_link]) == 0)
		return -1;

	nr_reloc = (int)(shdr->sh_size / shdr->sh_entsize);
	switch (shdr->sh_type) {
	case SHT_REL:
		err = relocate_section_rel(sym_table, (Elf32_Rel *)rel_data,
					   target_sect, nr_reloc);
		break;

	case SHT_RELA:
		err = relocate_section_rela(sym_table, (Elf32_Rela *)rel_data,
					    target_sect, nr_reloc);
		break;

	default:
		err = -1;
		break;
	}
	return err;
}

/*
 * Load ELF relocatable file.
 */
static int
load_reloc(Elf32_Ehdr *ehdr, task_t task, int fd, void **entry)
{
	Elf32_Shdr *shdr;
	char *buf;
	void *base, *addr, *mapped;
	size_t shdr_size, total_size;
	int i, err = 0;

	/* Read section header */
	shdr_size = ehdr->e_shentsize * ehdr->e_shnum;
	if ((buf = malloc(shdr_size)) == NULL)
		return ENOMEM;

	if (lseek(fd, ehdr->e_shoff, SEEK_SET) < 0) {
		err = EIO;
		goto out1;
	}
	if (read(fd, buf, shdr_size) < 0) {
		err = EIO;
		goto out1;
	}
	/* Allocate memory for text, data and bss. */
	shdr = (Elf32_Shdr *)buf;
	total_size = 0;
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_NOBITS) { /* bss? */
			total_size = shdr->sh_addr + shdr->sh_size;
			break;
		}
	}
	if (total_size == 0) {
		err = ENOEXEC;
		goto out1;
	}
	if (vm_allocate(task, &base, total_size, 1) != 0) {
		err = ENOMEM;
		goto out1;
	}

	if (vm_map(task, base, total_size, &mapped) != 0) {
		err = ENOMEM;
		goto out1;
	}
	/* Copy sections */
	shdr = (Elf32_Shdr *)buf;
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		/*
		 *DPRINTF(("section: type=%x addr=%x size=%d offset=%x flags=%x\n",
		 *   shdr->sh_type, shdr->sh_addr, shdr->sh_size,
		 *   shdr->sh_offset, shdr->sh_flags));
		 */
		sect_addr[i] = 0;
		if (shdr->sh_type == SHT_PROGBITS) {
			switch (shdr->sh_flags & SHF_VALID) {
			case (SHF_ALLOC | SHF_EXECINSTR): /* text */
			case (SHF_ALLOC | SHF_WRITE): /* data */
			case SHF_ALLOC: /* rodata */
				break;
			default:
				continue;
			}
			addr = (char *)((u_long)mapped + shdr->sh_addr);
			if (shdr->sh_size == 0) {
				continue;
			}
		} else if (shdr->sh_type == SHT_NOBITS) {
			/* bss */
			sect_addr[i] = \
				(char *)((u_long)mapped + shdr->sh_addr);
			continue;
		} else if (shdr->sh_type == SHT_SYMTAB ||
			   shdr->sh_type == SHT_RELA ||
			   shdr->sh_type == SHT_REL)
		{

			if ((addr = malloc(shdr->sh_size)) == NULL) {
				err = ENOMEM;
				goto out2;
			}
		} else
			continue;

		if (lseek(fd, shdr->sh_offset, SEEK_SET) < 0) {
			err = EIO;
			goto out2;
		}
		if (read(fd, addr, shdr->sh_size) < 0) {
			err = EIO;
			goto out2;
		}
		sect_addr[i] = addr;
	}
	/* Process relocation */
	shdr = (Elf32_Shdr *)buf;
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) {
			if (relocate_section(shdr, sect_addr[i]) != 0) {
				err = EIO;
				DPRINTF(("exec: relocation failed\n"));
				goto out2;
			}
		}
	}
	*entry = (void *)((u_long)mapped + ehdr->e_entry);
 out2:
	/* Release symbol table */
	shdr = (Elf32_Shdr *)buf;
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_SYMTAB ||
		    shdr->sh_type == SHT_RELA ||
		    shdr->sh_type == SHT_REL) {
			if (sect_addr[i])
				free(sect_addr[i]);
		}
	}
	vm_free(task_self(), mapped);
 out1:
	free(buf);
	return err;
}

#endif /* !CONFIG_MMU */


/*
 * Load ELF file
 */
int
elf_load(void *header, task_t task, int fd, void **entry)
{
	int err;

#ifdef CONFIG_MMU
	err = load_exec((Elf32_Ehdr *)header, task, fd, entry);
#else
	err = load_reloc((Elf32_Ehdr *)header, task, fd, entry);
#endif
	return err;
}

/*
 * Probe ELF file
 */
int
elf_probe(void *header)
{
	Elf32_Ehdr *ehdr;

	/* Check ELF header */
	ehdr = (Elf32_Ehdr *)header;
	if ((ehdr->e_ident[EI_MAG0] != ELFMAG0) ||
	    (ehdr->e_ident[EI_MAG1] != ELFMAG1) ||
	    (ehdr->e_ident[EI_MAG2] != ELFMAG2) ||
	    (ehdr->e_ident[EI_MAG3] != ELFMAG3))
		return -1;
#ifdef CONFIG_MMU
	if (ehdr->e_type != ET_EXEC)
		return -1;
#else
	if (ehdr->e_type != ET_REL)
		return -1;
#endif
	return 0;
}

void
elf_init(void)
{
}
