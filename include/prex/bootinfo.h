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

#ifndef _PREX_BOOTINFO_H
#define _PREX_BOOTINFO_H

#include <sys/types.h>

/*
 * Video information
 */
struct vidinfo
{
	int	pixel_x;	/* screen pixels */
	int	pixel_y;
	int	text_x;		/* text size, in characters */
	int	text_y;
};

/*
 * Module information for kernel, driver, and boot tasks.
 * An OS loader will build this structure regardless of its file format.
 */
struct module
{
	char	name[16];	/* name of image */
	paddr_t	phys;		/* physical address */
	size_t	size;		/* size of image */
	vaddr_t	entry;		/* entry address */
	vaddr_t	text;		/* text address */
	vaddr_t	data;		/* data address */
	size_t	textsz;		/* text size */
	size_t	datasz;		/* data size */
	size_t	bsssz;		/* bss size */
};

/*
 * Physical memory
 */
struct physmem
{
	paddr_t	base;		/* start address */
	size_t	size;		/* size in bytes */
	int	type;		/* type */
};

/* memory types */
#define MT_USABLE	1
#define MT_MEMHOLE	2
#define MT_RESERVED	3
#define MT_BOOTDISK	4

#define NMEMS		16	/* max number of memory slots */

/*
 * Boot information
 */
struct bootinfo
{
	struct vidinfo	video;
	struct physmem	ram[NMEMS];	/* physical ram table */
	int		nr_rams;	/* number of ram blocks */
	struct physmem	bootdisk;	/* boot disk in memory */
	int		nr_tasks;	/* number of boot tasks */
	struct module	kernel;		/* kernel image */
	struct module	driver;		/* driver image */
	struct module	tasks[1];	/* boot tasks image */
};

#define BOOTINFO_SIZE	1024	/* max size of boot information */

#endif /* !_PREX_BOOTINFO_H */
