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

#include <prex/prex.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>

#include "devfs.h"


#define devfs_mount	((vfsop_mount_t)vfs_nullop)
#define devfs_unmount	((vfsop_umount_t)vfs_nullop)
#define devfs_sync	((vfsop_sync_t)vfs_nullop)
#define devfs_vget	((vfsop_vget_t)vfs_nullop)
#define devfs_statfs	((vfsop_statfs_t)vfs_nullop)

static int devfs_open	(vnode_t, int);
static int devfs_close	(vnode_t, file_t);
static int devfs_read	(vnode_t, file_t, void *, size_t, size_t *);
static int devfs_write	(vnode_t, file_t, void *, size_t, size_t *);
#define devfs_seek	((vnop_seek_t)vop_nullop)
static int devfs_ioctl	(vnode_t, file_t, u_long, void *);
#define devfs_fsync	((vnop_fsync_t)vop_nullop)
static int devfs_readdir(vnode_t, file_t, struct dirent *);
static int devfs_lookup	(vnode_t, char *, vnode_t);
#define devfs_create	((vnop_create_t)vop_einval)
#define devfs_remove	((vnop_remove_t)vop_einval)
#define devfs_rename	((vnop_rename_t)vop_einval)
#define devfs_mkdir	((vnop_mkdir_t)vop_einval)
#define devfs_rmdir	((vnop_rmdir_t)vop_einval)
#define devfs_getattr	((vnop_getattr_t)vop_nullop)
#define devfs_setattr	((vnop_setattr_t)vop_nullop)
#define devfs_inactive	((vnop_inactive_t)vop_nullop)
#define devfs_truncate	((vnop_truncate_t)vop_nullop)

struct vnops devfs_vnops;

/*
 * File system operations
 */
struct vfsops devfs_vfsops = {
	devfs_mount,		/* mount */
	devfs_unmount,		/* unmount */
	devfs_sync,		/* sync */
	devfs_vget,		/* vget */
	devfs_statfs,		/* statfs */
	&devfs_vnops,		/* vnops */
};

/*
 * vnode operations
 */
struct vnops devfs_vnops = {
	devfs_open,		/* open */
	devfs_close,		/* close */
	devfs_read,		/* read */
	devfs_write,		/* write */
	devfs_seek,		/* seek */
	devfs_ioctl,		/* ioctl */
	devfs_fsync,		/* fsync */
	devfs_readdir,		/* readdir */
	devfs_lookup,		/* lookup */
	devfs_create,		/* create */
	devfs_remove,		/* remove */
	devfs_rename,		/* remame */
	devfs_mkdir,		/* mkdir */
	devfs_rmdir,		/* rmdir */
	devfs_getattr,		/* getattr */
	devfs_setattr,		/* setattr */
	devfs_inactive,		/* inactive */
	devfs_truncate,		/* truncate */
};

static int
devfs_open(vnode_t vp, int flags)
{
	char *path;
	device_t dev;
	int err;

	DPRINTF(("devfs_open: path=%s\n", vp->v_path));

	path = vp->v_path;
	if (!strcmp(path, "/"))	/* root ? */
		return 0;

	if (*path == '/')
		path++;
	err = device_open(path, flags & DO_RWMASK, &dev);
	if (err) {
		DPRINTF(("devfs_open: can not open device = %s error=%d\n",
			 path, err));
		return err;
	}
	vp->v_data = (void *)dev;	/* Store private data */
	return 0;
}

static int
devfs_close(vnode_t vp, file_t fp)
{

	DPRINTF(("devfs_close: fp=%x\n", fp));

	if (!strcmp(vp->v_path, "/"))	/* root ? */
		return 0;

	return device_close((device_t)vp->v_data);
}

static int
devfs_read(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	int err;
	size_t len;

	len = size;
	err = device_read((device_t)vp->v_data, buf, &len, fp->f_offset);
	if (!err)
		*result = len;
	return err;
}

static int
devfs_write(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	int err;
	size_t len;

	len = size;
	err = device_write((device_t)vp->v_data, buf, &len, fp->f_offset);
	if (!err)
		*result = len;
	DPRINTF(("devfs_write: err=%d len=%d\n", err, len));
	return err;
}

static int
devfs_ioctl(vnode_t vp, file_t fp, u_long cmd, void *arg)
{
	DPRINTF(("devfs_ioctl\n"));
	return EINVAL;
}

static int
devfs_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	struct info_device info;
	int err, i;

	DPRINTF(("devfs_lookup:%s\n", name));

	if (*name == '\0')
		return ENOENT;

	i = 0;
	err = 0;
	info.cookie = 0;
	for (;;) {
		err = sys_info(INFO_DEVICE, &info);
		if (err)
			return ENOENT;
		if (!strncmp(info.name, name, MAXDEVNAME))
			break;
		i++;
	}
	vp->v_type = (info.flags & DF_CHR) ? VCHR : VBLK;
	vp->v_mode = (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
			      | S_IROTH | S_IWOTH);
	return 0;
}

/*
 * @vp: vnode of the directory.
 */
static int
devfs_readdir(vnode_t vp, file_t fp, struct dirent *dir)
{
	struct info_device info;
	int err, i;

	DPRINTF(("devfs_readdir offset=%d\n", fp->f_offset));

	i = 0;
	err = 0;
	info.cookie = 0;
	do {
		err = sys_info(INFO_DEVICE, &info);
		if (err)
			return ENOENT;
	} while (i++ != fp->f_offset);

	dir->d_type = 0;
	if (info.flags & DF_CHR)
		dir->d_type = DT_CHR;
	else if (info.flags & DF_BLK)
		dir->d_type = DT_BLK;
	strcpy((char *)&dir->d_name, info.name);
	dir->d_fileno = fp->f_offset;
	dir->d_namlen = strlen(dir->d_name);

	DPRINTF(("devfs_readdir: %s\n", dir->d_name));
	fp->f_offset++;
	return 0;
}

int
devfs_init(void)
{
	return 0;
}
