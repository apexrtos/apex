/*-
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
 * conf.c - File system configuration.
 */

#include <prex/prex.h>
#include <sys/mount.h>

#include <limits.h>
#include <unistd.h>
#include <string.h>

#include "vfs.h"

extern struct vfsops ramfs_vfsops;
extern struct vfsops devfs_vfsops;
extern struct vfsops arfs_vfsops;
extern struct vfsops fifofs_vfsops;
extern struct vfsops fatfs_vfsops;

extern int ramfs_init(void);
extern int devfs_init(void);
extern int arfs_init(void);
extern int fifofs_init(void);
extern int fatfs_init(void);

/*
 * VFS switch table
 */
const struct vfssw vfssw_table[] = {
#ifdef CONFIG_RAMFS
	{"ramfs",	ramfs_init,	&ramfs_vfsops}, /* RAM */
#endif
#ifdef CONFIG_DEVFS
	{"devfs",	devfs_init,	&devfs_vfsops},	/* device fs */
#endif
#ifdef CONFIG_ARFS
	{"arfs",	arfs_init,	&arfs_vfsops},	/* archive fs */
#endif
#ifdef CONFIG_FIFOFS
	{"fifofs",	fifofs_init,	&fifofs_vfsops}, /* FIFO & pipe */
#endif
#ifdef CONFIG_FATFS
	{"fatfs",	fatfs_init,	&fatfs_vfsops},	/* DOS FAT */
#endif
	{NULL, NULL, NULL},
};
