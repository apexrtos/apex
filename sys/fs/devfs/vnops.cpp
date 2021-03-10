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
 * devfs - device file system.
 *
 * This covers the list of devices registered in the kernel and access to them
 * from the file system.
 *
 * The current design is a small step towards the long term goal of fully
 * merging devices into the file system code, following the "everything is
 * a file" design philosophy. Because of this, the code below is a bit smelly,
 * especially when dealing with the vnode lock.
 */

#include <cassert>
#include <compiler.h>
#include <conf/config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <device.h>
#include <dirent.h>
#include <errno.h>
#include <fs/file.h>
#include <fs/mount.h>
#include <fs/util.h>
#include <fs/vnode.h>
#include <sys/stat.h>

#define dfsdbg(...)

/*
 * list of devices
 */
static list device_list;
static spinlock device_list_lock;

static int devfs_init();
static int devfs_open(file *, int, mode_t);
static int devfs_close(file *);
static ssize_t devfs_read(file *, const iovec *, size_t, off_t);
static ssize_t devfs_write(file *, const iovec *, size_t, off_t);
static int devfs_seek (file *, off_t, int);
static int devfs_ioctl(file *, u_long, void *);
static int devfs_readdir(file *, dirent *, size_t);
static int devfs_lookup(vnode *, const char *, size_t, vnode *);
static int devfs_inactive(vnode *);

/*
 * vnode operations
 */
static const vnops devfs_vnops = {
	.vop_open = devfs_open,
	.vop_close = devfs_close,
	.vop_read = devfs_read,
	.vop_write = devfs_write,
	.vop_seek = devfs_seek,
	.vop_ioctl = devfs_ioctl,
	.vop_fsync = ((vnop_fsync_fn)vop_nullop),
	.vop_readdir = devfs_readdir,
	.vop_lookup = devfs_lookup,
	.vop_mknod = ((vnop_mknod_fn)vop_einval),
	.vop_unlink = ((vnop_unlink_fn)vop_einval),
	.vop_rename = ((vnop_rename_fn)vop_einval),
	.vop_getattr = ((vnop_getattr_fn)vop_nullop),
	.vop_setattr = ((vnop_setattr_fn)vop_nullop),
	.vop_inactive = devfs_inactive,
	.vop_truncate = ((vnop_truncate_fn)vop_nullop),
};

/*
 * File system operations
 */
static const vfsops devfs_vfsops = {
	.vfs_init = devfs_init,
	.vfs_mount = ((vfsop_mount_fn)vfs_nullop),
	.vfs_umount = ((vfsop_umount_fn)vfs_nullop),
	.vfs_sync = ((vfsop_sync_fn)vfs_nullop),
	.vfs_vget = ((vfsop_vget_fn)vfs_nullop),
	.vfs_statfs = ((vfsop_statfs_fn)vfs_nullop),
	.vfs_vnops = &devfs_vnops,
};

static int
devfs_init()
{
	list_init(&device_list);
	spinlock_init(&device_list_lock);

	return 0;
}

static int
devfs_open(file *fp, int flags, mode_t mode)
{
	dfsdbg("devfs_open: fp=%p name=%s\n", fp, fp->f_vnode->v_name);

	/*
	 * Root is opened for directory reading.
	 */
	if (fp->f_vnode->v_flags & VROOT)
		return 0;

	device *dev = (device *)fp->f_vnode->v_data;

	/*
	 * Device may have been destroyed.
	 */
	if (!dev)
		return -ENODEV;

	/*
	 * Stash device info in f_data for driver use.
	 */
	fp->f_data = dev->info;

	/*
	 * Call open function if the device has registered one.
	 */
	if (!dev->devio->open)
		return 0;

	++dev->busy;
	vn_unlock(fp->f_vnode);

	const int r = (*dev->devio->open)(fp);

	vn_lock(fp->f_vnode);
	--dev->busy;

	return r;
}

static int
devfs_close(file *fp)
{
	dfsdbg("devfs_close: fp=%p name=%s\n", fp, fp->f_vnode->v_name);

	/*
	 * Root has no device context.
	 */
	if (fp->f_vnode->v_flags & VROOT)
		return 0;

	device *dev = (device *)fp->f_vnode->v_data;

	/*
	 * Device may have been destroyed.
	 */
	if (!dev)
		return -ENODEV;

	/*
	 * Call close function if the device has registered one.
	 */
	if (!dev->devio->close)
		return 0;

	++dev->busy;
	vn_unlock(fp->f_vnode);

	int r = (*dev->devio->close)(fp);

	vn_lock(fp->f_vnode);
	--dev->busy;

	return r;
}

static ssize_t
devfs_read(file *fp, const iovec *iov, size_t count,
    off_t offset)
{
	device *dev = (device *)fp->f_vnode->v_data;

	/*
	 * Device may have been destroyed.
	 */
	if (!dev)
		return -ENODEV;

	if (!dev->devio->read)
		return DERR(-ENOTSUP);

	++dev->busy;
	vn_unlock(fp->f_vnode);

	int r = (*dev->devio->read)(fp, iov, count, offset);

	vn_lock(fp->f_vnode);
	--dev->busy;

	return r;
}

static ssize_t
devfs_write(file *fp, const iovec *iov, size_t count,
    off_t offset)
{
	device *dev = (device *)fp->f_vnode->v_data;

	/*
	 * Device may have been destroyed.
	 */
	if (!dev)
		return -ENODEV;

	if (!dev->devio->write)
		return DERR(-ENOTSUP);

	++dev->busy;
	vn_unlock(fp->f_vnode);

	int r = (*dev->devio->write)(fp, iov, count, offset);

	vn_lock(fp->f_vnode);
	--dev->busy;

	return r;
}

static int
devfs_seek(file *fp, off_t off, int whence)
{
	device *dev = (device *)fp->f_vnode->v_data;

	/*
	 * Device may have been destroyed.
	 */
	if (!dev)
		return -ENODEV;

	/* Seek is optional */
	if (!dev->devio->seek)
		return 0;

	++dev->busy;
	vn_unlock(fp->f_vnode);

	int r = (*dev->devio->seek)(fp, off, whence);

	vn_lock(fp->f_vnode);
	--dev->busy;

	return r;
}

static int
devfs_ioctl(file *fp, u_long cmd, void *arg)
{
	device *dev = (device *)fp->f_vnode->v_data;

	/*
	 * Device may have been destroyed.
	 */
	if (!dev)
		return -ENODEV;

	if (!dev->devio->ioctl)
		return DERR(-ENOTSUP);

	++dev->busy;
	vn_unlock(fp->f_vnode);

	int r = (*dev->devio->ioctl)(fp, cmd, arg);

	vn_lock(fp->f_vnode);
	--dev->busy;

	return r;
}

static int
devfs_readdir(file *fp, dirent *buf, size_t len)
{
	device *d;
	size_t i = 1;
	size_t remain = len;

	if (fp->f_offset == 0) {
		if (dirbuf_add(&buf, &remain, 0, fp->f_offset, DT_DIR, "."))
			goto out;
		++fp->f_offset;
	}

	if (fp->f_offset == 1) {
		if (dirbuf_add(&buf, &remain, 0, fp->f_offset, DT_DIR, ".."))
			goto out;
		++fp->f_offset;
	}

	/* REVISIT: this can return inconsistent results if a device_create or
	 * device_destroy happens between calls to devfs_readdir. */
	spinlock_lock(&device_list_lock);
	list_for_each_entry(d, &device_list, link) {
		if (++i != fp->f_offset)
			continue;

		unsigned char t = DT_UNKNOWN;
		if (d->flags & DF_CHR)
			t = DT_CHR;
		else if (d->flags & DF_BLK)
			t = DT_BLK;

		if (d->devio &&
		    dirbuf_add(&buf, &remain, 0, fp->f_offset, t, d->name))
			break;

		++fp->f_offset;
	}
	spinlock_unlock(&device_list_lock);

out:
	if (remain != len)
		return len - remain;

	return -ENOENT;
}

static int
devfs_lookup(vnode *dvp, const char *name, size_t name_len, vnode *vp)
{
	device *dev;

	dfsdbg("devfs_lookup: (%zu):%s\n", name_len, name);

	spinlock_lock(&device_list_lock);
	list_for_each_entry(dev, &device_list, link) {
		if (name_len >= ARRAY_SIZE(dev->name) || dev->name[name_len] ||
		    !dev->devio || strncmp(dev->name, name, name_len))
			continue;
		vp->v_data = dev;
		vp->v_mode = ((dev->flags & DF_CHR) ? S_IFCHR : S_IFBLK) |
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
		assert(!dev->vnode);
		dev->vnode = vp;
		spinlock_unlock(&device_list_lock);

		return 0;
	}
	spinlock_unlock(&device_list_lock);

	return -ENOENT;
}

static int
devfs_inactive(vnode *vp)
{
	device *dev = (device *)vp->v_data;

	if (dev) {
		assert(!dev->busy);
		dev->vnode = 0;
	}

	return 0;
}

/*
 * device_create - create a device and add to device list
 */
device *
device_create(const devio *io, const char *name, int flags, void *info)
{
	device *dev;
	size_t len;

	len = strlen(name);
	if (len == 0 || len >= ARRAY_SIZE(((device *)0)->name))	/* Invalid name? */
		return 0;

	spinlock_lock(&device_list_lock);

	list_for_each_entry(dev, &device_list, link) {
		if (strcmp(dev->name, name) == 0) {
			/* device name in use */
			spinlock_unlock(&device_list_lock);
			return 0;
		}
	}
	if (!(dev = (device *)malloc(sizeof(*dev)))) {
		spinlock_unlock(&device_list_lock);
		return 0;
	}
	strcpy(dev->name, name);
	dev->flags = flags;
	dev->vnode = nullptr;
	dev->busy = 0;
	dev->devio = io;
	dev->info = info;
	list_insert(&device_list, &dev->link);

	spinlock_unlock(&device_list_lock);
	return dev;
}

/*
 * device_reserve - reserve a device name
 */
device *
device_reserve(const char *name, bool indexed)
{
	device *dev;
	char namei[ARRAY_SIZE(((device *)0)->name)];

	if (!indexed)
		return device_create(nullptr, name, 0, nullptr);

	for (size_t i = 0; i < 100; ++i) {
		snprintf(namei, ARRAY_SIZE(namei), "%s%d", name, i);
		if ((dev = device_create(nullptr, namei, 0, nullptr)))
			return dev;
	}

	return nullptr;
}

/*
 * device_attach - attach reserved device name to device instance
 */
void
device_attach(device *dev, const devio *io, int flags, void *info)
{
	spinlock_lock(&device_list_lock);
	assert(!dev->devio);

	dev->flags = flags;
	dev->devio = io;
	dev->info = info;
	spinlock_unlock(&device_list_lock);
}

/*
 * device_hide - remove a device from device list and hide associated vnode
 *
 * Once a device has been removed no more operations can be started on it.
 */
void
device_hide(device *dev)
{
	spinlock_lock(&device_list_lock);
	list_remove(&dev->link);
	spinlock_unlock(&device_list_lock);

	vnode *vp = dev->vnode;
	if (vp) {
		vn_lock(vp);
		vn_hide(vp);
		vn_unlock(vp);
	}

#if defined(CONFIG_DEBUG)
	list_init(&dev->link);
#endif
}

/*
 * device_busy - check if a any operations are running on device
 */
bool
device_busy(device *dev)
{
	bool r = false;
	vnode *vp = dev->vnode;

	if (vp) {
		vn_lock(vp);
		r = dev->busy != 0;
		vn_unlock(vp);
	}

	return r;
}

/*
 * device_destroy - release device memory
 *
 * A device must be hidden and have no running operations before destroying.
 */
void
device_destroy(device *dev)
{
#if defined(CONFIG_DEBUG)
	assert(list_empty(&dev->link));
#endif
	assert(dev->busy == 0);

	vnode *vp = dev->vnode;

	if (vp) {
		vn_lock(vp);
		vp->v_data = nullptr;
		vn_unlock(vp);
	}

	free(dev);
}

REGISTER_FILESYSTEM(devfs);
