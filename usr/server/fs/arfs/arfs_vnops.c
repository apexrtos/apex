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

#include <prex/prex.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/buf.h>

#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <ar.h>

#include "arfs.h"

#define arfs_open	((vnop_open_t)vop_nullop)
#define arfs_close	((vnop_close_t)vop_nullop)
static int arfs_read	(vnode_t, file_t, void *, size_t, size_t *);
#define arfs_write	((vnop_write_t)vop_nullop)
static int arfs_seek	(vnode_t, file_t, off_t, off_t);
#define arfs_ioctl	((vnop_ioctl_t)vop_einval)
#define arfs_fsync	((vnop_fsync_t)vop_nullop)
static int arfs_readdir	(vnode_t, file_t, struct dirent *);
static int arfs_lookup	(vnode_t, char *, vnode_t);
#define arfs_create	((vnop_create_t)vop_einval)
#define arfs_remove	((vnop_remove_t)vop_einval)
#define arfs_rename	((vnop_rename_t)vop_einval)
#define arfs_mkdir	((vnop_mkdir_t)vop_einval)
#define arfs_rmdir	((vnop_rmdir_t)vop_einval)
#define arfs_getattr	((vnop_getattr_t)vop_nullop)
#define arfs_setattr	((vnop_setattr_t)vop_nullop)
#define arfs_inactive	((vnop_inactive_t)vop_nullop)
#define arfs_truncate	((vnop_truncate_t)vop_nullop)

static char iobuf[BSIZE*2];

#if CONFIG_FS_THREADS > 1
static mutex_t arfs_lock = MUTEX_INITIALIZER;
#endif

/*
 * vnode operations
 */
const struct vnops arfs_vnops = {
	arfs_open,		/* open */
	arfs_close,		/* close */
	arfs_read,		/* read */
	arfs_write,		/* write */
	arfs_seek,		/* seek */
	arfs_ioctl,		/* ioctl */
	arfs_fsync,		/* fsync */
	arfs_readdir,		/* readdir */
	arfs_lookup,		/* lookup */
	arfs_create,		/* create */
	arfs_remove,		/* remove */
	arfs_rename,		/* remame */
	arfs_mkdir,		/* mkdir */
	arfs_rmdir,		/* rmdir */
	arfs_getattr,		/* getattr */
	arfs_setattr,		/* setattr */
	arfs_inactive,		/* inactive */
	arfs_truncate,		/* truncate */
};

/*
 * Read blocks.
 * iobuf is filled by read data.
 */
static int
arfs_readblk(mount_t mp, int blkno)
{
	struct buf *bp;
	int err;

	/*
	 * Read two blocks for archive header
	 */
	if ((err = bread(mp->m_dev, blkno, &bp)) != 0)
		return err;
	memcpy(iobuf, bp->b_data, BSIZE);
	brelse(bp);

	if ((err = bread(mp->m_dev, blkno + 1, &bp)) != 0)
		return err;
	memcpy(iobuf + BSIZE, bp->b_data, BSIZE);
	brelse(bp);

	return 0;
}

/*
 * Lookup vnode for the specified file/directory.
 * The vnode is filled properly.
 */
static int
arfs_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	struct ar_hdr *hdr;
	int blkno, err;
	off_t off;
	size_t size;
	mount_t mp;
	char *p;

	DPRINTF(("arfs_lookup: name=%s\n", name));
	if (*name == '\0')
		return ENOENT;

	mutex_lock(&arfs_lock);

	err = ENOENT;
	mp = vp->v_mount;
	blkno = 0;
	off = SARMAG;	/* offset in archive image */
	for (;;) {
		/* Read two blocks for archive header */
		if (arfs_readblk(mp, blkno) != 0)
			goto out;

		/* Check file header */
		hdr = (struct ar_hdr *)(iobuf + (off % BSIZE));
		if (strncmp(hdr->ar_fmag, ARFMAG, sizeof(ARFMAG) - 1))
			goto out;

		/* Get file size */
		size = (size_t)atol((char *)&hdr->ar_size);
		if (size == 0)
			goto out;

		/* Convert archive name */
		if ((p = memchr(&hdr->ar_name, '/', 16)) != NULL)
			*p = '\0';

		if (strncmp(name, (char *)&hdr->ar_name, 16) == 0)
			break;

		/* Proceed to next archive header */
		off += (sizeof(struct ar_hdr) + size);
		off += (off % 2); /* Pad to even boundary */

		blkno = (int)(off / BSIZE);
	}
	vp->v_type = VREG;

	/* No write access */
	vp->v_mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	vp->v_size = size;
	vp->v_blkno = blkno;
	vp->v_data = (void *)(off + sizeof(struct ar_hdr));
	err = 0;
 out:
	mutex_unlock(&arfs_lock);
	DPRINTF(("arfs_lookup: err=%d\n\n", err));
	return err;
}

static int
arfs_read(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	off_t off, file_pos, buf_pos;
	int blkno, err;
	size_t nr_read, nr_copy;
	mount_t mp;
	struct buf *bp;

	DPRINTF(("arfs_read: start size=%d\n", size));
	mutex_lock(&arfs_lock);

	*result = 0;
	mp = vp->v_mount;

	/* Check if current file position is already end of file. */
	file_pos = fp->f_offset;
	if (file_pos >= (off_t)vp->v_size) {
		err = 0;
		goto out;
	}
	/* Get the actual read size. */
	if (vp->v_size - file_pos < size)
		size = vp->v_size - file_pos;

	/* Read and copy data */
	off = (off_t)vp->v_data;
	nr_read = 0;
	for (;;) {
		DPRINTF(("arfs_read: file_pos=%d buf=%x size=%d\n",
			 file_pos, buf, size));

		blkno = (off + file_pos) / BSIZE;
		buf_pos = (off + file_pos) % BSIZE;
		if ((err = bread(mp->m_dev, blkno, &bp)) != 0)
			goto out;
		nr_copy = BSIZE;
		if (buf_pos > 0)
			nr_copy -= buf_pos;
		if (buf_pos + size < BSIZE)
			nr_copy = size;
		ASSERT(nr_copy > 0);
		memcpy(buf, bp->b_data + buf_pos, nr_copy);
		brelse(bp);

		file_pos += nr_copy;
		DPRINTF(("arfs_read: file_pos=%d nr_copy=%d\n",
			 file_pos, nr_copy));

		nr_read += nr_copy;
		size -= nr_copy;
		if (size <= 0)
			break;
		buf = (void *)((u_long)buf + nr_copy);
		buf_pos = 0;
	}
	fp->f_offset = file_pos;
	*result = nr_read;
	err = 0;
 out:
	mutex_unlock(&arfs_lock);
	DPRINTF(("arfs_read: err=%d\n\n", err));
	return err;
}

/*
 * Check if the seek offset is valid.
 */
static int
arfs_seek(vnode_t vp, file_t fp, off_t oldoff, off_t newoff)
{

	if (newoff > (off_t)vp->v_size)
		return -1;

	return 0;
}

static int
arfs_readdir(vnode_t vp, file_t fp, struct dirent *dir)
{
	struct ar_hdr *hdr;
	int blkno, i, err;
	off_t off;
	size_t size;
	mount_t mp;
	char *p;

	DPRINTF(("arfs_readdir: start\n"));
	mutex_lock(&arfs_lock);

	i = 0;
	mp = vp->v_mount;
	blkno = 0;
	off = SARMAG;	/* offset in archive image */
	for (;;) {
		/* Read two blocks for archive header */
		if ((err = arfs_readblk(mp, blkno)) != 0)
			goto out;

		hdr = (struct ar_hdr *)(iobuf + (off % BSIZE));

		/* Get file size */
		size = (size_t)atol((char *)&hdr->ar_size);
		if (size == 0) {
			err = ENOENT;
			goto out;
		}
		if (i == fp->f_offset)
			break;

		/* Proceed to next archive header */
		off += (sizeof(struct ar_hdr) + size);
		off += (off % 2); /* Pad to even boundary */

		blkno = off / BSIZE;
		i++;
	}

	/* Convert archive name */
	if ((p = memchr(&hdr->ar_name, '/', 16)) != NULL)
		*p = '\0';

	strcpy((char *)&dir->d_name, (char *)&hdr->ar_name);
	dir->d_namlen = strlen(dir->d_name);
	dir->d_fileno = fp->f_offset;
	dir->d_type = DT_REG;

	fp->f_offset++;
	err = 0;
 out:
	mutex_unlock(&arfs_lock);
	return err;
}


int
arfs_init(void)
{
	return 0;
}
