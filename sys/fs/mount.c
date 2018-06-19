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
 * mount.c - mount operations
 */

#include "mount.h"

#include "debug.h"
#include "vfs.h"
#include "vnode.h"
#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <fs.h>
#include <kmem.h>
#include <string.h>
#include <sync.h>
#include <sys/stat.h>
#include <task.h>

/*
 * List of mount points.
 */
static struct list mount_list = LIST_INIT(mount_list);

/*
 * Global mount point lock.
 */
static struct mutex mount_mutex;

/*
 * mount_init - initialise mount data structures
 */
void mount_init(void)
{
	mutex_init(&mount_mutex);
}

/*
 * fs_lookup - lookup file system.
 */
static const struct vfssw *
fs_lookup(const char *name)
{
	extern const struct vfssw __filesystems, __filesystems_end;

	for (const struct vfssw *fs = &__filesystems; fs != &__filesystems_end; ++fs) {
		if (!strcmp(name, fs->vs_name))
			return fs;
	}

	return NULL;
}

/*
 * do_root_mount - mount root file system
 */
static int do_root_mount(struct mount *mp, unsigned long flags,
    const void *data)
{
	int err;
	struct vnode *vp_root;

	/* get root node */
	if (!(vp_root = vget(mp, NULL, "", 0)))
		return DERR(-ENOMEM);
	vp_root->v_flags = VROOT;
	vp_root->v_mode = S_IFDIR;

	/* configure mount point */
	mp->m_covered = NULL;
	mp->m_root = vp_root;

	/* mount the file system */
	if ((err = VFS_MOUNT(mp, flags, data)) < 0)
		goto err;

	/* unlock root node, keep ref */
	vn_unlock(vp_root);
	return 0;

err:
	vput(vp_root);
	return err;
}

/*
 * do_mount - mount file system
 */
static int do_mount(struct mount *mp, const char *dir, unsigned long flags,
    const void *data)
{
	int err;
	struct vnode *vp_covered, *vp_root;

	if ((err = lookup_t(task_cur(), AT_FDCWD, dir, &vp_covered, NULL, NULL, 0)))
		return err;

	/* fail if not a directory */
	if (!S_ISDIR(vp_covered->v_mode)) {
		err = DERR(-ENOTDIR);
		goto err;
	}

	/* get root node */
	/* We don't strictly need to lock/unlock vp_covered->v_parent here as
	   we are guaranteed vp_covered->v_parent cannot be removed due to the
	   lock we hold on vp_covered. However, strict validation in vnode.c
	   does not expect this. */
	vn_lock(vp_covered->v_parent);
	if (!(vp_root = vget(mp, vp_covered->v_parent, vp_covered->v_name,
	    strlen(vp_covered->v_name)))) {
		vn_unlock(vp_covered->v_parent);
		err = DERR(-ENOMEM);
		goto err;
	}
	vn_unlock(vp_covered->v_parent);
	vp_root->v_flags = VROOT;
	vp_root->v_mode = S_IFDIR;

	/* configure mount point */
	mp->m_covered = vp_covered;
	mp->m_root = vp_root;

	/* mount the file system */
	if ((err = VFS_MOUNT(mp, flags, data)) < 0)
		goto err1;

	/* hide covered vnode, keep ref */
	vn_hide(vp_covered);
	vn_unlock(vp_covered);

	/* unlock root node, keep ref */
	vn_unlock(vp_root);
	return 0;

err1:
	vput(vp_root);
err:
	vput(vp_covered);
	return err;
}

/*
 * mount
 */
int
mount(const char *dev, const char *dir, const char *type, unsigned long flags,
    const void *data)
{
	const struct vfssw *fs;
	dev_t device;
	int devfd = -1;
	int err;
	struct mount *mp;

	info("VFS: Mounting %s dev=%s dir=%s\n", type, dev, dir);

	if (!dir || *dir == '\0')
		return DERR(-ENOENT);

	/* find a file system. */
	if (!(fs = fs_lookup(type)))
		return DERR(-ENODEV);

	/* open device, NULL device is valid (e.g. ramfs)
	   also accept dev == type */
	device = 0;
	if (dev && *dev && strcmp(dev, type)) {
		struct stat st;

		if ((devfd = kopen(dev, O_RDWR | O_CLOEXEC)) < 0)
			return devfd;
		if ((err = kfstat(devfd, &st)))
			goto err0;
		if (!S_ISBLK(st.st_mode)) {
			err = DERR(-ENOTBLK);
			goto err0;
		}

		device = st.st_rdev;
	}

	/* create mount entry */
	if (!(mp = kmem_alloc(sizeof(struct mount), MEM_NORMAL))) {
		err = DERR(-ENOMEM);
		goto err1;
	}
	*mp = (struct mount) {
		.m_op = fs->vs_op,
		.m_flags = flags,
		.m_count = 0,
		.m_devfd = devfd,
	};

	mutex_lock(&mount_mutex);

	/* fail if device already mounted */
	if (device) {
		const struct mount *mi;
		list_for_each_entry(mi, &mount_list, m_link) {
			struct stat st;

			if (mi->m_devfd < 0)
				continue;

			if ((err = kfstat(mi->m_devfd, &st)))
				goto err2;
			if (device == st.st_rdev) {
				err = DERR(-EBUSY);
				goto err2;
			}
		}
	}

	/* root mount is special as no vnodes exist yet */
	if (dir[0] == '/' && !dir[1])
		err = do_root_mount(mp, flags, data);
	else
		err = do_mount(mp, dir, flags, data);

	if (err)
		goto err2;

	/* insert into mount list */
	list_insert(&mount_list, &mp->m_link);
	mutex_unlock(&mount_mutex);
	return 0;

err2:
	mutex_unlock(&mount_mutex);
err1:
	kmem_free(mp);
err0:
	kclose(devfd);
	return err;
}

/*
 * umount2
 */
int
umount(const char *path)
{
	return umount2(path, 0);
}

int
umount2(const char *path, int flags)
{
	int err;
	struct mount *mp;
	struct vnode *vp;

	vdbgsys("umount: path=%s\n", path);

	if ((err = lookup_t(task_cur(), AT_FDCWD, path, &vp, NULL, NULL, 0)))
		return err;

	/* can't unmount if nothing mounted */
	if (!(vp->v_flags & VROOT)) {
		vput(vp);
		return -EINVAL;
	}

	mutex_lock(&mount_mutex);
	mp = vp->v_mount;

	assert(vp == mp->m_root);
	vput(vp);

	/* can't unmount root file system */
	if (!mp->m_covered) {
		err = -EINVAL;
		goto out;
	}

	/* can't unmount with vnodes in use */
	if (mp->m_count > 1) {
		err = DERR(-EBUSY);
		goto out;
	}

	if ((err = VFS_UMOUNT(mp)))
		goto out;
	list_remove(&mp->m_link);

	/* unhide covered vnode */
	vn_lock(mp->m_covered);
	vn_unhide(mp->m_covered);
	vput(mp->m_covered);

	/* release root vnode */
	vn_lock(mp->m_root);
	vput(mp->m_root);
	assert(!mp->m_count);

#ifdef CONFIG_BIO
	/* flush all buffers */
	binval(mp->m_devfd);
#endif

	if (mp->m_devfd >= 0)
		kclose(mp->m_devfd);
	kmem_free(mp);

out:
	mutex_unlock(&mount_mutex);
	return err;
}

/*
 * sync
 */
void
sync(void)
{
	struct mount *mp;

	/* Call each mounted file system. */
	mutex_lock(&mount_mutex);
	list_for_each_entry(mp, &mount_list, m_link)
		VFS_SYNC(mp);
	mutex_unlock(&mount_mutex);

#ifdef CONFIG_BIO
	bio_sync();
#endif
}

/*
 * Mark a mount point as busy.
 */
void
vfs_busy(struct mount *mp)
{
	mutex_lock(&mount_mutex);
	mp->m_count++;
	mutex_unlock(&mount_mutex);
}

/*
 * Mark a mount point as busy.
 */
void
vfs_unbusy(struct mount *mp)
{
	mutex_lock(&mount_mutex);
	mp->m_count--;
	mutex_unlock(&mount_mutex);
}

int
vfs_nullop(void)
{
	return 0;
}

int
vfs_einval(void)
{
	return -EINVAL;
}

/*
 * mount_dump - dump mount data
 */
void
mount_dump(void)
{
	struct mount *mp;

	mutex_lock(&mount_mutex);
	info("Dump mount data\n");
	info(" devfd count root    \n");
	info(" ----- ----- --------\n");
	list_for_each_entry(mp, &mount_list, m_link)
		info(" %5d %5d %p\n",
		    mp->m_devfd, mp->m_count, mp->m_root);
	mutex_unlock(&mount_mutex);
}
