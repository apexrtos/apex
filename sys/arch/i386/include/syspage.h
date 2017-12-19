/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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

#ifndef _I386_SYSPAGE_H
#define _I386_SYSPAGE_H

/**
 * syspage layout:
 *
 * +------------------+ SYSPAGE_BASE
 * | NULL pointer     |
 * | detection page   |
 * |                  |
 * +------------------+ +0x1000
 * | Interrupt stack  |
 * | (PTE0 for boot)  |
 * |                  |
 * +------------------+ +0x2000
 * | Boot information |
 * +------------------+ +0x2400
 * | Boot stack       |
 * +------------------+ +0x3000
 * | PGD for boot     |
 * | (MMU only)       |
 * |                  |
 * +------------------+ +0x4000
 *
 * Note: Interrupt stack should be placed after NULL page
 * to detect the stack overflow.
 */

#define INTSTACK_BASE	(SYSPAGE_BASE + 0x1000)
#define BOOT_PTE0	(SYSPAGE_BASE + 0x1000)
#define BOOTINFO_BASE	(SYSPAGE_BASE + 0x2000)
#define BOOTSTACK_BASE	(SYSPAGE_BASE + 0x2400)
#define BOOT_PGD	(SYSPAGE_BASE + 0x3000)

#define BOOT_PTE0_PHYS	0x1000
#define BOOT_PGD_PHYS	0x3000

#define INTSTACK_SIZE	0x1000
#define BOOTSTACK_SIZE	0x0c00

#define INTSTACK_TOP	(INTSTACK_BASE + INTSTACK_SIZE)
#define BOOTSTACK_TOP	(BOOTSTACK_BASE + BOOTSTACK_SIZE)

#ifdef CONFIG_MMU
#define SYSPAGE_SIZE	0x4000
#else
#define SYSPAGE_SIZE	0x3000
#endif

#endif /* !_I386_SYSPAGE_H */
