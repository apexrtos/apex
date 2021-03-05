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

#include "ramfs.h"

#include <debug.h>
#include <errno.h>
#include <fs/mount.h>
#include <fs/vnode.h>
#include <sys/stat.h>

/*
 * Mount a file system.
 */
static int
ramfs_mount(struct mount *mp, int flags, const void *data)
{
	struct ramfs_node *np;

	/* Create a root node */
	if (!(np = ramfs_allocate_node("", 1, S_IFDIR)))
		return DERR(-ENOMEM);
	mp->m_root->v_data = np;
	return 0;
}

/*
 * Unmount a file system.
 *
 * TODO: Currently, we don't support unmounting of the RAMFS. This is
 *       because we have to deallocate all nodes included in all sub
 *       directories, and it requires more work...
 */
static int
ramfs_umount(struct mount *mp)
{
	return DERR(-EBUSY);
}

/*
 * File system operations
 */
static const vfsops ramfs_vfsops = {
	.vfs_init = ((vfsop_init_fn)vfs_nullop),
	.vfs_mount = ramfs_mount,
	.vfs_umount = ramfs_umount,
	.vfs_sync = ((vfsop_sync_fn)vfs_nullop),
	.vfs_vget = ((vfsop_vget_fn)vfs_nullop),
	.vfs_statfs = ((vfsop_statfs_fn)vfs_nullop),
	.vfs_vnops = &ramfs_vnops,
};

REGISTER_FILESYSTEM(ramfs);
