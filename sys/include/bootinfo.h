/*
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * Boot information
 *
 * The boot information is stored by an OS loader, and
 * it is refered by kernel later in boot time.
 */

#ifndef bootinfo_h
#define bootinfo_h

#include <sys/include/types.h>

/*
 * Physical memory
 */
struct boot_mem {
	phys	       *base;	    /* start address */
	uint32_t	size;	    /* size in bytes */
	uint32_t	type;	    /* MT_... */
};

/* memory type */
#define MT_NONE		0	    /* unused entry */
#define MT_NORMAL	1	    /* normal memory e.g. DRAM */
#define MT_FAST		2	    /* fast memory e.g. SOC SRAM */
#define MT_MEMHOLE	3	    /* memory hole - no physical backing */
#define MT_KERNEL	4	    /* kernel text/data */
#define MT_BOOTDISK	5	    /* bootdisk image */
#define MT_RESERVED	6	    /* reserved memory e.g. boot stack */
#define MT_DMA		7	    /* uncached memory suitable for DMA */

/*
 * Boot information
 */
struct bootinfo {
	uint32_t	nr_rams;    /* number of ram blocks */
	struct boot_mem	ram[16];    /* physical ram table */
};

#endif /* !bootinfo_h */
