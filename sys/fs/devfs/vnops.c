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
 */

#include <compiler.h>
#include <debug.h>
#include <device.h>
#include <dirent.h>
#include <errno.h>
#include <fs/file.h>
#include <fs/mount.h>
#include <fs/util.h>
#include <fs/vnode.h>
#include <string.h>
#include <sys/stat.h>

#define dfsdbg(...)

static int devfs_open(struct file *, int, mode_t);
static int devfs_close(struct file *);
static ssize_t devfs_read(struct file *, const struct iovec *, size_t);
static ssize_t devfs_write(struct file *, const struct iovec *, size_t);
static int devfs_ioctl(struct file *, u_long, void *);
static int devfs_readdir(struct file *, struct dirent *, size_t);
static int devfs_lookup(struct vnode *, const char *, size_t, struct vnode *);
static int devfs_inactive(struct vnode *);

/*
 * vnode operations
 */
static const struct vnops devfs_vnops = {
	.vop_open = devfs_open,
	.vop_close = devfs_close,
	.vop_read = devfs_read,
	.vop_write = devfs_write,
	.vop_seek = ((vnop_seek_fn)vop_nullop),
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
static const struct vfsops devfs_vfsops = {
	((vfsop_init_fn)vfs_nullop),	/* init */
	((vfsop_mount_fn)vfs_nullop),	/* mount */
	((vfsop_umount_fn)vfs_nullop),	/* unmount */
	((vfsop_sync_fn)vfs_nullop),	/* sync */
	((vfsop_vget_fn)vfs_nullop),	/* vget */
	((vfsop_statfs_fn)vfs_nullop),	/* statfs */
	&devfs_vnops,			/* vnops */
};

static int
devfs_open(struct file *fp, int flags, mode_t mode)
{
	dfsdbg("devfs_open: fp=%p name=%s\n", fp, fp->f_vnode->v_name);

	/*
	 * Root is opened for directory reading.
	 */
	if (fp->f_vnode->v_flags & VROOT)
		return 0;

	/*
	 * Otherwise we must have a device pointer.
	 */
	struct device *dev = (struct device *)fp->f_vnode->v_data;

	/*
	 * Stash device info in f_data for driver use.
	 */
	fp->f_data = dev->info;

	/*
	 * Call open function if the device has registered one.
	 */
	if (dev->devio->open)
		return (*dev->devio->open)(fp);

	return 0;
}

static int
devfs_close(struct file *fp)
{
	dfsdbg("devfs_close: fp=%p name=%s\n", fp, fp->f_vnode->v_name);

	/*
	 * Root has no device context.
	 */
	if (fp->f_vnode->v_flags & VROOT)
		return 0;

	struct device *dev = fp->f_vnode->v_data;

	/*
	 * Call close function if the device has registered one.
	 */
	if (dev->devio->close)
		return (*dev->devio->close)(fp);

	return 0;
}

static ssize_t
devfs_read(struct file *fp, const struct iovec *iov, size_t count)
{
	struct device *dev = fp->f_vnode->v_data;

	if (!dev->devio->read)
		return -EIO;

	return (*dev->devio->read)(fp, iov, count);
}

static ssize_t
devfs_write(struct file *fp, const struct iovec *iov, size_t count)
{
	struct device *dev = fp->f_vnode->v_data;

	if (!dev->devio->write)
		return -EIO;

	return (*dev->devio->write)(fp, iov, count);
}

static int
devfs_ioctl(struct file *fp, u_long cmd, void *arg)
{
	struct device *dev = fp->f_vnode->v_data;

	if (!dev->devio->ioctl)
		return -EIO;

	return (*dev->devio->ioctl)(fp, cmd, arg);
}

static int
devfs_readdir(struct file *fp, struct dirent *buf, size_t len)
{
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

	while (1) {
		/* TODO(efficiency): n^2 here with device_info */
		struct device *dev;
		char name[ARRAY_SIZE(dev->name)];
		int flags;
		if (device_info(fp->f_offset - 2, &flags, name))
			goto out;

		unsigned char d_type = DT_UNKNOWN;
		if (flags & DF_CHR)
			d_type = DT_CHR;
		else if (flags & DF_BLK)
			d_type = DT_BLK;

		if (dirbuf_add(&buf, &remain, 0, fp->f_offset, d_type, name))
			goto out;

		++fp->f_offset;
	}

out:
	if (remain != len)
		return len - remain;

	return -ENOENT;
}

static int
devfs_lookup(struct vnode *dvp, const char *name, size_t name_len, struct vnode *vp)
{
	struct device *dev;

	dfsdbg("devfs_lookup: (%zu):%s\n", name_len, name);

	/* TODO: handle name_len when devfs merges with kernel device layer */
	if (!(dev = device_lookup(name))) {
		vp->v_data = NULL;
		return -ENOENT;
	}

	vp->v_data = dev;
	vp->v_mode = ((dev->flags & DF_CHR) ? S_IFCHR : S_IFBLK) |
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
	return 0;
}

static int
devfs_inactive(struct vnode *vp)
{
	if (vp->v_data)
		device_release(vp->v_data);
	return 0;
}

REGISTER_FILESYSTEM(devfs);
