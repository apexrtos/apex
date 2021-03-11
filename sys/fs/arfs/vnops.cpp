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
 * arfs_vnops.c - vnode operations for archive file system.
 */

/**
 * General design:
 *
 * ARFS (ARchive File System) is the read-only file system which
 * handles the generic archive (*.a) file as a file system image.
 * The file system is typically used for the boot time file system,
 * and it's mounted to the ram disk device mapped to the pre-loaded
 * archive file image. All files are placed in one single directory.
 */

#include <ar.h>
#include <compiler.h>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <dirent.h>
#include <errno.h>
#include <fs.h>
#include <fs/file.h>
#include <fs/mount.h>
#include <fs/util.h>
#include <fs/vnode.h>
#include <sys/param.h>
#include <sys/uio.h>

#define afsdbg(...)

/*
 * Read an extended filename from archive header
 */
static bool
read_extended_filename(int fd, const size_t off, char *buf, size_t len)
{
	char *p;
	ssize_t rd;
	size_t size;
	ar_hdr h;
	if (kpread(fd, &h, sizeof h, SARMAG) != sizeof h)
		return false;
	if (strncmp(h.ar_fmag, ARFMAG, sizeof ARFMAG  - 1))
		return false;
	if (h.ar_name[0] != '/' || h.ar_name[1] != '/')
		return false;
	size = atol(h.ar_size);
	if (off >= size)
		return false;
	if ((rd = kpread(fd, buf, MIN(size - off, len), SARMAG + sizeof h + off)) < 0)
		return false;
	buf[(size_t)rd == len ? len - 1 : rd] = 0;
	if (!(p = (char *)memchr(buf, '/', rd)))
		return false;
	*p = 0;
	return true;
}

/*
 * Mount file system.
 */
static int
arfs_mount(struct mount *mp, int flags, const void *data)
{
	char buf[SARMAG];
	int err = 0;

	/* Read first block */
	err = kpread(mp->m_devfd, buf, SARMAG, 0);
	if (err < 0) {
		afsdbg("arfs_mount: read error=%d\n", -err);
		return err;
	}

	/* Check if the device includes valid archive image. */
	if (strncmp(buf, ARMAG, SARMAG)) {
		afsdbg("arfs_mount: invalid archive image!\n");
		return DERR(-EINVAL);
	}

	/* Ok, we find the archive */
	mp->m_flags |= MS_RDONLY;
	return 0;
}

/*
 * Lookup vnode for the specified file/directory.
 * The vnode is filled properly.
 */
static int
arfs_lookup(vnode *dvp, const char *name, const size_t name_len,
    vnode *vp)
{
	ar_hdr h;
	const struct mount *mp = vp->v_mount;
	size_t off = SARMAG;
	size_t size;
	char *ar_name;
	char name_buf[128];

	afsdbg("arfs_lookup: name=(%zu):%s\n", name_len, name);

	/* File name too long - impossible to match */
	if (name_len >= sizeof name_buf - 1)
		return DERR(-ENOENT);

	for (;;) {
		int err;

		if ((err = kpread(mp->m_devfd, &h, sizeof(h), off)) < 0)
			return err;
		if (err == 0)
			return DERR(-ENOENT);

		/* Check file header */
		if (strncmp(h.ar_fmag, ARFMAG, sizeof(ARFMAG) - 1))
			return DERR(-EIO);

		/* Get file size */
		size = atol(h.ar_size);

		if (h.ar_name[0] == '/') {
			/* filename directory */
			if (h.ar_name[1] == '/')
				goto next;
			/* extended filename */
			if (!read_extended_filename(mp->m_devfd,
			    atol(h.ar_name + 1), name_buf, sizeof name_buf))
				return DERR(-EIO);
			ar_name = name_buf;
		} else
			ar_name = h.ar_name;

		/* Compare file name */
		if (!strncmp(name, ar_name, name_len) &&
		    (!ar_name[name_len] || ar_name[name_len] == '/'))
			break;

		/* Proceed to next archive header */
next:
		off += sizeof(h) + size;
		off += off % 2; /* Pad to even boundary */
	}

	/* No write access */
	vp->v_mode = S_IFREG | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	vp->v_size = size;
	vp->v_data = (void *)(off + sizeof(ar_hdr));

	return 0;
}

static ssize_t
arfs_read(file *fp, void *buf, size_t size, off_t offset)
{
	const vnode *vp = fp->f_vnode;
	const struct mount *mp = vp->v_mount;
	size_t off = (size_t)vp->v_data;
	ssize_t res;

	afsdbg("arfs_read: start size=%d\n", size);

	/* Check if current file position is already end of file. */
	if (offset >= vp->v_size)
		return 0;

	/* Get the actual read size. */
	if (vp->v_size - offset < size)
		size = vp->v_size - offset;

	/* Read data */
	if ((res = kpread(mp->m_devfd, buf, size, off + offset)) < 0)
		return res;

	return res;
}

static ssize_t
arfs_read_iov(file *fp, const iovec *iov, size_t count,
    off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [fp](std::span<std::byte> buf, off_t offset) {
		return arfs_read(fp, data(buf), size(buf), offset);
	});
}

static int
arfs_readdir(file *fp, dirent *buf, size_t len)
{
	const struct mount *mp = fp->f_vnode->v_mount;
	size_t remain = len;
	int err = 0;
	char name_buf[128];

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

	size_t i = 2;
	off_t off = SARMAG;	/* offset in archive image */
	for (;;) {
		ar_hdr h;

		/* read file header */
		if ((err = kpread(mp->m_devfd, &h, sizeof(h), off)) <= 0)
			goto out;

		/* Check file header */
		if (strncmp(h.ar_fmag, ARFMAG, sizeof(ARFMAG) - 1)) {
			err = DERR(-EIO);
			goto out;
		}

		/* Get file size */
		size_t size = atol(h.ar_size);

		if (i == fp->f_offset) {
			/* Convert archive name */
			char *p;
			if (!(p = (char *)memchr(h.ar_name, '/', ARRAY_SIZE(h.ar_name)))) {
				err = DERR(-EIO);
				goto out;
			}
			const char *name = 0;
			if (p != h.ar_name) {
				*p = 0;
				name = h.ar_name;
			} else if (h.ar_name[1] != '/') {
				/* Extended filename */
				if (!read_extended_filename(mp->m_devfd,
				    atol(h.ar_name + 1), name_buf, sizeof name_buf))
					return DERR(-EIO);
				name = name_buf;
			}
			if (name && dirbuf_add(&buf, &remain, 0, fp->f_offset, DT_REG, name))
				goto out;
			++fp->f_offset;
		}

		/* Proceed to next archive header */
		off += sizeof(h) + size;
		off += off % 2; /* Pad to even boundary */

		++i;
	}

out:
	if (remain != len)
		return len - remain;

	return err;
}

/*
 * vnode operations
 */
const vnops arfs_vnops = {
	.vop_open = (vnop_open_fn)vop_nullop,
	.vop_close = (vnop_close_fn)vop_nullop,
	.vop_read = arfs_read_iov,
	.vop_write = (vnop_write_fn)vop_nullop,
	.vop_seek = (vnop_seek_fn)vop_nullop,
	.vop_ioctl = (vnop_ioctl_fn)vop_einval,
	.vop_fsync = (vnop_fsync_fn)vop_nullop,
	.vop_readdir = arfs_readdir,
	.vop_lookup = arfs_lookup,
	.vop_mknod = (vnop_mknod_fn)vop_einval,
	.vop_unlink = (vnop_unlink_fn)vop_einval,
	.vop_rename = (vnop_rename_fn)vop_einval,
	.vop_getattr = (vnop_getattr_fn)vop_nullop,
	.vop_setattr = (vnop_setattr_fn)vop_nullop,
	.vop_inactive = (vnop_inactive_fn)vop_nullop,
	.vop_truncate = (vnop_truncate_fn)vop_nullop,
};

/*
 * File system operations
 */
static const vfsops arfs_vfsops = {
	.vfs_init = (vfsop_init_fn)vfs_nullop,
	.vfs_mount = arfs_mount,
	.vfs_umount = (vfsop_umount_fn)vfs_nullop,
	.vfs_sync = (vfsop_sync_fn)vfs_nullop,
	.vfs_vget = (vfsop_vget_fn)vfs_nullop,
	.vfs_statfs = (vfsop_statfs_fn)vfs_nullop,
	.vfs_vnops = &arfs_vnops,
};

REGISTER_FILESYSTEM(arfs);
