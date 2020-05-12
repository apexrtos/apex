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

#ifndef boot_h
#define boot_h

#include <conf/config.h>
#include <machine.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/include/bootargs.h>

#if defined(CONFIG_DEBUG)
#define dbg(...) debug_printf(__VA_ARGS__)
#else
#define dbg(...)
#endif

#if defined(CONFIG_INFO)
#define info(...) debug_printf(__VA_ARGS__)
#else
#define info(...)
#endif

extern struct bootargs args;
extern void (*kernel_entry)(phys *archive_addr, long archive_size,
    long machdep0, long machdep1);

int load_elf(const phys *img);
int load_bootimg();
void debug_puts(const char *);
void debug_printf(const char *, ...)
	__attribute__((format (printf, 1, 2)));
noreturn void panic(const char *);

#endif /* !boot_h */

