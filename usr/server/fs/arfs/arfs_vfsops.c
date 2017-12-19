/*
 * Copyright (c) 2006-2007, Kohsuke Ohtani
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
 * arfs_vfsops.c - vfs operations for archive file system.
 */

#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/buf.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ar.h>

#include "arfs.h"

extern struct vnops arfs_vnops;


static int arfs_mount	(mount_t mp, char *dev, int flags, void *data);
static int arfs_unmount	(mount_t mp);
#define arfs_sync	((vfsop_sync_t)vfs_nullop)
#define arfs_vget	((vfsop_vget_t)vfs_nullop)
#define arfs_statfs	((vfsop_statfs_t)vfs_nullop)

/*
 * File system operations
 */
const struct vfsops arfs_vfsops = {
	arfs_mount,		/* mount */
	arfs_unmount,		/* unmount */
	arfs_sync,		/* sync */
	arfs_vget,		/* vget */
	arfs_statfs,		/* statfs */
	&arfs_vnops,		/* vnops */
};

/*
 * Mount a file system.
 */
static int
arfs_mount(mount_t mp, char *dev, int flags, void *data)
{
	size_t size;
	char *buf;
	int err = 0;

	DPRINTF(("arfs_mount: dev=%s\n", dev));

	if ((buf = malloc(BSIZE)) == NULL)
		return ENOMEM;

	/* Read first block */
	size = BSIZE;
	err = device_read((device_t)mp->m_dev, buf, &size, 0);
	if (err) {
		DPRINTF(("arfs_mount: read error=%d\n", err));
		goto out;
	}

	/* Check if the device includes valid archive image. */
	if (strncmp(buf, ARMAG, SARMAG)) {
		DPRINTF(("arfs_mount: invalid archive image!\n"));
		err = EINVAL;
		goto out;
	}

	/* Ok, we find the archive */
	mp->m_flags |= MNT_RDONLY;
 out:
	free(buf);
	return err;
}

static int
arfs_unmount(mount_t mp)
{
	return 0;
}
