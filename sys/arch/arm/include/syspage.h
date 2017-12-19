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

#ifndef _ARM_SYSPAGE_H
#define _ARM_SYSPAGE_H

/**
 * syspage layout:
 *
 * +------------------+ SYSPAGE_BASE
 * | Vector page      |
 * |                  |
 * +------------------+ +0x1000
 * | Interrupt stack  |
 * |                  |
 * +------------------+ +0x2000
 * | Sys mode stack   |
 * |                  |
 * +------------------+ +0x3000
 * | Boot information |
 * +------------------+ +0x3400
 * | Abort mode stack |
 * +------------------+ +0x3800
 * | Boot stack       |
 * +------------------+ +0x4000
 * | PGD for boot     |
 * | (MMU only)       |
 * |                  |
 * +------------------+ +0x8000
 * | PTE0 for boot    |
 * | (MMU only)       |
 * +------------------+ +0x9000
 * | PTE1 for UART I/O|
 * | (MMU only)       |
 * +------------------+ +0xA000

 *
 * Note1: Kernel PGD must be stored at 16k aligned address.
 *
 * Note2: PTE0 must be stored at 4k aligned address.
 *
 * Note2: Interrupt stack should be placed after NULL page
 * to detect the stack overflow.
 */

#define INTSTACK_BASE	(SYSPAGE_BASE + 0x1000)
#define SYSSTACK_BASE	(SYSPAGE_BASE + 0x2000)
#define BOOTINFO_BASE	(SYSPAGE_BASE + 0x3000)
#define ABTSTACK_BASE	(SYSPAGE_BASE + 0x3400)
#define BOOTSTACK_BASE	(SYSPAGE_BASE + 0x3800)
#define BOOT_PGD	(SYSPAGE_BASE + 0x4000)
#define BOOT_PTE0	(SYSPAGE_BASE + 0x8000)
#define BOOT_PTE1	(SYSPAGE_BASE + 0x9000)

#define BOOT_PGD_PHYS	0x4000
#define BOOT_PTE0_PHYS	0x8000
#define BOOT_PTE1_PHYS	0x9000


#define INTSTACK_SIZE	0x1000
#define SYSSTACK_SIZE	0x1000
#define ABTSTACK_SIZE	0x400
#define BOOTSTACK_SIZE	0x800

#define INTSTACK_TOP	(INTSTACK_BASE + INTSTACK_SIZE)
#define SYSSTACK_TOP	(SYSSTACK_BASE + SYSSTACK_SIZE)
#define ABTSTACK_TOP	(ABTSTACK_BASE + ABTSTACK_SIZE)
#define BOOTSTACK_TOP	(BOOTSTACK_BASE + BOOTSTACK_SIZE)

#ifdef CONFIG_MMU
#define SYSPAGE_SIZE	0xA000
#else
#define SYSPAGE_SIZE	0x4000
#endif

#endif /* !_ARM_SYSPAGE_H */
