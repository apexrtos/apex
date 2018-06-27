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
 * main.c - boot loader main module
 */
#include <boot.h>

#include <machine.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

/*
 * Boot information
 */
struct bootinfo *bootinfo;
void (*kernel_entry)(struct bootinfo*);

void
bootinfo_dump(void)
{
	const char *strtype[] = { "", "NORMAL", "FAST", "MEMHOLE", "KERNEL",
	    "BOOTDISK", "RESERVED" };

	dbg("[Boot Information]\n");
	dbg("nr_rams=%d\n", bootinfo->nr_rams);
	for (size_t i = 0; i < bootinfo->nr_rams; i++) {
		if (bootinfo->ram[i].type != 0) {
			dbg("ram[%d]:  base=0x%08x size=0x%08x type=%s\n",
			    i,
			    (int)bootinfo->ram[i].base,
			    (int)bootinfo->ram[i].size,
			    strtype[bootinfo->ram[i].type]);
		}
	}
	dbg("\n");
}

/*
 * Debugging
 */
void
debug_puts(const char *s)
{
	while (*s)
		machine_putc(*s++);
}

void
debug_printf(const char *fmt, ...)
{
	char buf[256];

	va_list ap;
	va_start(ap, fmt);
	const int n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	if (n < 0) {
		debug_puts("*** Error, debug vsnprintf\n");
		return;
	}

	if (n >= sizeof buf) {
		debug_puts("*** Error, debug string too long\n");
		return;
	}

	debug_puts(buf);
}


/*
 * C entry point
 */
noreturn void
loader_main(void)
{
	/*
	 * Allocate bootinfo structure and set bootinfo pointer
	 */
	struct bootinfo bi = {};
	bootinfo = &bi;

	/*
	 * Setup minimum hardware for boot & initialise bootinfo
	 */
	machine_setup();

	/*
	 * Print banner
	 */
	info("APEX boot loader v1.00\n");

	/*
	 * Load program image
	 */
	if (machine_load_image() < 0)
		panic("failed to load kernel");

	/*
	 * Jump to the kernel entry point
	 */
	dbg("Kernel entry point %p\n", kernel_entry);
	info("Entering kernel...\n\n");
	(*kernel_entry)(bootinfo);
	__builtin_unreachable();
}
