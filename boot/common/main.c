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
 * main.c - Prex boot loader main module
 */

#include <boot.h>
#include <ar.h>

/*
 * Boot information
 */
struct bootinfo *const bootinfo = (struct bootinfo *)BOOTINFO_BASE;

#if defined(DEBUG) && defined(DEBUG_BOOTINFO)
static void
dump_image(struct module *m)
{

	printf("%x %x %x %x %x %x %x %x %s\n",
	       (int)m->entry, (int)m->phys, (int)m->size,
	       (int)m->text, (int)m->data, (int)m->textsz,
	       (int)m->datasz, (int)m->bsssz, m->name);
}

static void
dump_bootinfo(void)
{
	static const char strtype[][9] = \
		{ "", "USABLE", "MEMHOLE", "RESERVED", "BOOTDISK" };
	struct module *m;
	int i;

	printf("[Boot information]\n");

	printf("nr_rams=%d\n", bootinfo->nr_rams);
	for (i = 0; i < bootinfo->nr_rams; i++) {
		if (bootinfo->ram[i].type != 0) {
			printf("ram[%d]:  base=%x size=%x type=%s\n", i,
			       (int)bootinfo->ram[i].base,
			       (int)bootinfo->ram[i].size,
			       strtype[bootinfo->ram[i].type]);
		}
	}

	printf("bootdisk: base=%x size=%x\n",
	       (int)bootinfo->bootdisk.base,
	       (int)bootinfo->bootdisk.size);

	printf("entry    phys     size     text     data     textsz   datasz   bsssz    module\n");
	printf("-------- -------- -------- -------- -------- -------- -------- -------- ------\n");
	dump_image(&bootinfo->kernel);
	dump_image(&bootinfo->driver);

	m = (struct module *)&bootinfo->tasks[0];
	for (i = 0; i < bootinfo->nr_tasks; i++, m++)
		dump_image(m);
}
#endif

/*
 * C entry point
 */
void
loader_main(void)
{
	paddr_t kernel_entry;

	DPRINTF(("Prex Boot Loader V1.00\n"));

	/*
	 * Initialize data.
	 */
	memset(bootinfo, 0, BOOTINFO_SIZE);

	load_base = 0;
	load_start = 0;
	nr_img = 0;

	/*
	 * Setup minimum hardware for boot.
	 */
	machine_setup();

	/*
	 * Load program image.
	 */
	setup_image();
#if defined(DEBUG) && defined(DEBUG_BOOTINFO)
	dump_bootinfo();
#endif
	/*
	 * Jump to the kernel entry point
	 * via machine-dependent code.
	 */
	kernel_entry = (paddr_t)phys_to_virt(bootinfo->kernel.entry);

	DPRINTF(("kernel_entry=%x\n", kernel_entry));
	DPRINTF(("Entering kernel...\n\n"));

	start_kernel(kernel_entry);

	/* NOTREACHED */
}
