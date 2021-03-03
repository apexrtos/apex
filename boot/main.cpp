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
struct bootargs args;
void (*kernel_entry)(phys *, long, long, long);

/*
 * Debugging
 */
void
debug_puts(const char *s)
{
#if defined(CONFIG_BOOT_CONSOLE)
	while (*s) {
		if (*s == '\n')
			machine_putc('\r');
		machine_putc(*s++);
	}
#endif
}

void
debug_printf(const char *fmt, ...)
{
#if defined(CONFIG_BOOT_CONSOLE)
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
#endif
}

noreturn void
panic(const char *msg)
{
#if defined(CONFIG_BOOT_CONSOLE)
	debug_puts("Panic: ");
	debug_puts(msg);
#endif
	machine_panic();
}

/*
 * __assert_fail - print assertion message and halt system
 */
noreturn void
__assert_fail(const char *expr, const char *file, int line, const char *func)
{
	debug_printf("Assertion failed: %s (%s: %s: %d)\n",
	    expr, file, func, line);
	machine_panic();
}

/*
 * C entry point
 */
noreturn void
loader_main(void)
{
	/*
	 * Setup minimum hardware for boot
	 */
	machine_setup();

	/*
	 * Print banner
	 */
	info("Apex boot loader v2.00\n");

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
	(*kernel_entry)(args.archive_addr, args.archive_size, args.machdep0,
	    args.machdep1);
	__builtin_unreachable();
}
