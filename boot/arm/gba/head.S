/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * head.S - low level platform support
 *
 * This file contains the code from crt0.S which is released under
 * public domain by Jeff Frohwein.
 */

/*-
 * Memory usage:
 *
 * 0x02000000 - 0x0203ffff : EWRAM (256K)
 * 0x03000000 - 0x03007fff : IWRAM (32K)
 * 0x04000000 - 0x040003ff : I/O Register (1K)
 * 0x05000000 - 0x050003ff : Palette RAM (1K)
 * 0x06000000 - 0x06017fff : VRAM (96K)
 *
 * Note:
 * 0x03007f00 - 0x03007fff is reserved by GBA BIOS.
 */

#include <conf/config.h>
#include <syspage.h>

#define ENTRY(x) .global x; .align; x##:

	.section ".text","ax"
	.code 32
/*
 * Loader start point
 */
ENTRY(boot_entry)
	b	rom_header_end

	/*
	 * GBA ROM header
	 */

	/* Nintendo Logo Character Data (8000004h) */
	.fill	156,1,0

	/* Game Title (80000A0h) */
	.byte	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte	0x00,0x00,0x00,0x00

	/* Game Code (80000ACh) */
	.byte	0x00,0x00,0x00,0x00

	/* Maker Code (80000B0h) */
	.byte	0x30,0x31

	/* Fixed Value (80000B2h) */
	.byte	0x96

	/* Main Unit Code (80000B3h) */
	.byte	0x00

	/* Device Type (80000B4h) */
	.byte	0x00

	/* Unused Data (7Byte) (80000B5h) */
	.byte	0x00,0x00,0x00,0x00,0x00,0x00,0x00

	/* Software Version No (80000BCh) */
	.byte	0x00

	/* Complement Check (80000BDh) */
	.byte	0xf0

	/* Checksum (80000BEh) */
	.byte	0x00,0x00

	.align
	.code 32

rom_header_end:
	b	start_vector

	.global __boot_method, __slave_number

__boot_method:
	.byte	0	/* boot method (0=ROM boot, 3=Multiplay boot) */
__slave_number:
	.byte	0	/* slave # (1=slave#1, 2=slave#2, 3=slave#3) */

	.byte	0
	.byte	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0
	.word	0

	.align
	.code 32
stack_top:	.word	BOOT_STACKTOP

start_vector:
	mov	r0, #0xd3		/* Enter SVC mode, Disable IRQ,FIQ */
	msr	cpsr_c, r0
	ldr	sp, stack_top

	adr	r0, thumb_mode + 1
	bx	r0
	.code 16
thumb_mode:

/*
 * Relocate the loader code
 */
	mov	r3, #0x40
	lsl	r3, #7	    /* r3 = 0x2000 (8K) */
	mov	r2, #3
	lsl	r2, #24     /* r2 = 0x3000000 */
	lsl	r1, r3, #14 /* r1 = 0x8000000 */

copy_loop:
	ldmia	r1!, {r0}
	stmia	r2!, {r0}
	sub	r3, #4
	bne	copy_loop

	ldr	r0, =loader_main
	bx	r0		/* Change to ARM mode */

	.code 32

/*
 * Start kernel
 */
ENTRY(start_kernel)
	bx	r0

/*
 * Put one character to Visual Boy Advance (VBA) emulator console.
 *
 * Important:
 * This BIOS call is not supported by actual GBA BIOS. So, you must
 * disable this function when you run it on actual GBA H/W.
 * Otherwise, it will hang.
 */
putbuf:
	.long	0

ENTRY(machine_putc)
#if defined(DEBUG) && defined(CONFIG_DIAG_VBA)
	ldr	r1, =putbuf
	str	r0, [r1]
	mov	r0, r1
	swi	$0xff0000
#endif
	mov	pc, lr

	.section .tail,"ax"
dummy:
	.byte	0xff

.end
