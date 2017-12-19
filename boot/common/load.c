/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * load.c - load the images of boot modules
 */

#include <boot.h>
#include <ar.h>

paddr_t load_base;	/* current load address */
paddr_t load_start;	/* start address for loading */
int nr_img;		/* number of module images */

/*
 * Load all images.
 * Return 0 on success, -1 on failure.
 */
static int
load_image(struct ar_hdr *hdr, struct module *m)
{
	char *c;

	if (strncmp((char *)&hdr->ar_fmag, ARFMAG, 2))
		return -1;

	strncpy((char *)&m->name, (char *)&hdr->ar_name, 16);
	c = (char *)&m->name;
	while (*c != '/' && *c != ' ')
		c++;
	*c = '\0';

 	DPRINTF(("loading: hdr=%x module=%x name=%s\n",
		 (int)hdr, (int)m, (char *)&m->name));

	if (elf_load((char *)hdr + sizeof(struct ar_hdr), m))
		panic("Load error");
	return 0;
}

#if defined(CONFIG_BOOTDISK) && defined(CONFIG_RAMDISK)
/*
 * Setup boot disk
 */
static void
setup_bootdisk(struct ar_hdr *hdr)
{
	paddr_t base;
	size_t size;
#ifndef CONFIG_ROMBOOT
	int i;
#endif

	if (strncmp((char *)&hdr->ar_fmag, ARFMAG, 2))
		return;
	size = (size_t)atol((char *)&hdr->ar_size);
	if (size == 0)
		return;

	base = (paddr_t)hdr + sizeof(struct ar_hdr);
	bootinfo->bootdisk.base = base;
	bootinfo->bootdisk.size = size;

#ifndef CONFIG_ROMBOOT
	i = bootinfo->nr_rams;
	bootinfo->ram[i].base = base;
	bootinfo->ram[i].size = size;
	bootinfo->ram[i].type = MT_BOOTDISK;
	bootinfo->nr_rams++;
#endif
	DPRINTF(("bootdisk base=%x size=%x\n", bootinfo->bootdisk.base,
		 bootinfo->bootdisk.size));
}
#endif

/*
 * Setup OS images - kernel, driver and boot tasks.
 *
 * It reads each module file image and copy it to the appropriate
 * memory area. The image is built as generic an archive (.a) file.
 *
 * The image information is stored into the boot information area.
 */
void
setup_image(void)
{
	char *hdr;
	struct module *m;
	char *magic;
	int i;
	long len;

	/*
	 *  Sanity check for archive image
	 */
	magic = (char *)virt_to_phys(BOOTIMG_BASE);
	if (strncmp(magic, ARMAG, 8))
		panic("Invalid OS image");

	/*
	 * Load kernel image
	 */
	hdr = (char *)((paddr_t)magic + 8);
	if (load_image((struct ar_hdr *)hdr, &bootinfo->kernel))
		panic("Can not load kernel");

	/*
	 * Load driver module
	 */
	len = atol((char *)&((struct ar_hdr *)hdr)->ar_size);
	if (len == 0)
		panic("Invalid driver image");
	hdr = (char *)((paddr_t)hdr + sizeof(struct ar_hdr) + len);
	if (load_image((struct ar_hdr *)hdr, &bootinfo->driver))
		panic("Can not load driver");

	/*
	 * Load boot tasks
	 */
	i = 0;
	m = (struct module *)&bootinfo->tasks[0];
	while (1) {
		/* Proceed to next archive header */
		len = atol((char *)&((struct ar_hdr *)hdr)->ar_size);
		if (len == 0)
			break;
		hdr = (char *)((paddr_t)hdr + sizeof(struct ar_hdr) + len);

		/* Pad to even boundary */
		hdr += ((paddr_t)hdr % 2);

		/* Check archive header */
		if (strncmp((char *)&((struct ar_hdr *)hdr)->ar_fmag,
			    ARFMAG, 2))
			break;

#if defined(CONFIG_BOOTDISK) && defined(CONFIG_RAMDISK)
		/* Load boot disk image */
		if (!strncmp((char *)&((struct ar_hdr *)hdr)->ar_name,
			    "ramdisk.a", 9)) {
			setup_bootdisk((struct ar_hdr *)hdr);
			continue;
		}
#endif
		/* Load task */
		if (load_image((struct ar_hdr *)hdr, m))
			break;
		i++;
		m++;
	}

	bootinfo->nr_tasks = i;

	if (bootinfo->nr_tasks == 0)
		panic("No boot task found!");

	/*
	 * Save information for boot modules.
	 * This includes kernel, driver, and boot tasks.
	 */
	i = bootinfo->nr_rams;
	bootinfo->ram[i].base = load_start;
	bootinfo->ram[i].size = (size_t)(load_base - load_start);
	bootinfo->ram[i].type = MT_RESERVED;
	bootinfo->nr_rams++;
}
