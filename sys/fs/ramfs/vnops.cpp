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
 * rmafs_vnops.c - vnode operations for RAM file system.
 */

#include "ramfs.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/file.h>
#include <fs/util.h>
#include <fs/vnode.h>
#include <kernel.h>
#include <page.h>
#include <sys/stat.h>

/*
 * Page ownership identifier for RAMFS
 */
static char ramfs_id;

/*
 * TODO: ramfs cleanup
 * - use hash map?
 * - use kernel list.h instead of self-rolled
 * - don't duplicate tests guaranteed by vfs
 */

static ssize_t ramfs_read_iov(file *, const iovec *, size_t, off_t);
static ssize_t ramfs_write_iov(file *, const iovec *, size_t, off_t);
static int ramfs_readdir(file *, dirent *, size_t);
static int ramfs_lookup(vnode *, const char *, size_t, vnode *);
static int ramfs_mknod(vnode *, const char *, size_t, int, mode_t);
static int ramfs_unlink(vnode *, vnode *);
static int ramfs_rename(vnode *, vnode *, vnode *, vnode *, const char *, size_t);
static int ramfs_truncate(vnode *);

/*
 * vnode operations
 */
const vnops ramfs_vnops = {
	.vop_open = ((vnop_open_fn)vop_nullop),
	.vop_close = ((vnop_close_fn)vop_nullop),
	.vop_read = ramfs_read_iov,
	.vop_write = ramfs_write_iov,
	.vop_seek = ((vnop_seek_fn)vop_nullop),
	.vop_ioctl = ((vnop_ioctl_fn)vop_einval),
	.vop_fsync = ((vnop_fsync_fn)vop_nullop),
	.vop_readdir = ramfs_readdir,
	.vop_lookup = ramfs_lookup,
	.vop_mknod = ramfs_mknod,
	.vop_unlink = ramfs_unlink,
	.vop_rename = ramfs_rename,
	.vop_getattr = ((vnop_getattr_fn)vop_nullop),
	.vop_setattr = ((vnop_setattr_fn)vop_nullop),
	.vop_inactive = ((vnop_inactive_fn)vop_nullop),
	.vop_truncate = ramfs_truncate,
};

ramfs_node *
ramfs_allocate_node(const char *name, size_t name_len, mode_t mode)
{
	char *rn_name;
	ramfs_node *np;

	if (!(np = malloc(sizeof(ramfs_node))))
		return NULL;
	if (!(rn_name = malloc(name_len + 1))) {
		free(np);
		return NULL;
	}

	memcpy(rn_name, name, name_len);
	rn_name[name_len] = 0;
	*np = (ramfs_node) {
		.rn_namelen = name_len,
		.rn_name = rn_name,
		.rn_mode = mode,
	};

	return np;
}

void
ramfs_free_node(ramfs_node *np)
{
	free(np->rn_name);
	free(np);
}

static ramfs_node *
ramfs_add_node(ramfs_node *dnp, const char *name, size_t name_len, mode_t mode)
{
	ramfs_node *np, *prev;

	if (!(np = ramfs_allocate_node(name, name_len, mode)))
		return NULL;

	/* Link to the directory list */
	if (dnp->rn_child == NULL) {
		dnp->rn_child = np;
	} else {
		prev = dnp->rn_child;
		while (prev->rn_next != NULL)
			prev = prev->rn_next;
		prev->rn_next = np;
	}
	return np;
}

static int
ramfs_remove_node(ramfs_node *dnp, ramfs_node *np)
{
	ramfs_node *prev;

	if (dnp->rn_child == NULL)
		return -ENOENT;

	/* Unlink from the directory list */
	if (dnp->rn_child == np) {
		dnp->rn_child = np->rn_next;
	} else {
		for (prev = dnp->rn_child; prev->rn_next != np;
		     prev = prev->rn_next) {
			if (prev->rn_next == NULL)
				return -ENOENT;
		}
		prev->rn_next = np->rn_next;
	}
	ramfs_free_node(np);
	return 0;
}

static int
ramfs_rename_node(ramfs_node *np, const char *name, size_t name_len)
{
	char *tmp;

	if (name_len <= np->rn_namelen) {
		/* Reuse current name buffer */
		strlcpy(np->rn_name, name, np->rn_namelen + 1);
	} else {
		/* Expand name buffer */
		if (!(tmp = malloc(name_len + 1)))
			return -ENOMEM;
		strlcpy(tmp, name, name_len + 1);
		free(np->rn_name);
		np->rn_name = tmp;
	}
	np->rn_namelen = name_len;
	return 0;
}

static int
ramfs_lookup(vnode *dvp, const char *name, size_t name_len, vnode *vp)
{
	ramfs_node *np;
	ramfs_node *dnp = dvp->v_data;
	int found;

	if (*name == '\0')
		return -ENOENT;

	found = 0;
	for (np = dnp->rn_child; np != NULL; np = np->rn_next) {
		if (np->rn_namelen == name_len &&
		    memcmp(name, np->rn_name, name_len) == 0) {
			found = 1;
			break;
		}
	}
	if (found == 0)
		return -ENOENT;
	vp->v_data = np;
	vp->v_mode = np->rn_mode;
	vp->v_size = np->rn_size;

	return 0;
}

/* Unlink a node */
static int
ramfs_unlink(vnode *dvp, vnode *vp)
{
	ramfs_node *np;

	rfsdbg("unlink %s in %s\n", vp->v_path, dvp->v_path);

	np = vp->v_data;

	if (np->rn_child)
		return -ENOTEMPTY;

	if (np->rn_buf != NULL) {
		if (np->rn_bufsize > PAGE_SIZE / 2)
			page_free(virt_to_phys(np->rn_buf), np->rn_bufsize,
				  &ramfs_id);
		else
			free(np->rn_buf);
		np->rn_buf = NULL; /* incase remove_node fails */
		np->rn_bufsize = 0;
		np->rn_size = 0;
	}
	vp->v_size = 0;
	return ramfs_remove_node(dvp->v_data, np);
}

/* Truncate file */
static int
ramfs_truncate(vnode *vp)
{
	ramfs_node *np = vp->v_data;

	rfsdbg("truncate %s\n", vp->v_path);
	if (np->rn_buf != NULL) {
		if (np->rn_bufsize > PAGE_SIZE / 2)
			page_free(virt_to_phys(np->rn_buf), np->rn_bufsize,
				  &ramfs_id);
		else
			free(np->rn_buf);
		np->rn_buf = NULL;
		np->rn_bufsize = 0;
		np->rn_size = 0;
	}
	vp->v_size = 0;
	return 0;
}

/*
 * Create file system node
 */
static int
ramfs_mknod(vnode *dvp, const char *name, size_t name_len, int flags, mode_t mode)
{
	ramfs_node *dnp = dvp->v_data;
	ramfs_node *np;

	rfsdbg("create (%zu):%s in %s\n", name_len, name, dvp->v_path);

	if (!(np = ramfs_add_node(dnp, name, name_len, mode)))
		return -ENOMEM;

	return 0;
}

static ssize_t
ramfs_read(file *fp, void *buf, size_t size, off_t offset)
{
	vnode *vp = fp->f_vnode;
	ramfs_node *np = fp->f_vnode->v_data;

	if (!S_ISREG(vp->v_mode) && !S_ISLNK(vp->v_mode))
		return -EINVAL;

	if (offset >= vp->v_size)
		return 0;

	if (vp->v_size - offset < size)
		size = vp->v_size - offset;

	memcpy(buf, np->rn_buf + offset, size);

	return size;
}

static ssize_t
ramfs_read_iov(file *fp, const iovec *iov, size_t count,
    off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [fp](std::span<std::byte> buf, off_t offset) {
		return ramfs_read(fp, data(buf), size(buf), offset);
	});
}

static int
ramfs_grow(ramfs_node *np, off_t new_size)
{
	void *new_buf = NULL;

	if (new_size <= np->rn_bufsize)
		return 0;

	/*
	 * We allocate small files using malloc. Once a file grows to more
	 * than half a page in size we switch to using page_alloc.
	 */
	if (new_size > PAGE_SIZE / 2) {
		new_size = PAGE_ALIGN(new_size);
		phys *const p = page_alloc(new_size, MA_NORMAL, &ramfs_id);
		if (!p)
			return -1;
		new_buf = phys_to_virt(p);
	} else {
		/* try not to fragment malloc too much */
		new_size = ALIGNn(new_size, 32);
		new_buf = malloc(new_size);
		if (!new_buf)
			return -1;
	}

	/* copy file data to new buffer */
	if (np->rn_size != 0)
		memcpy(new_buf, np->rn_buf, np->rn_size);

	/* free old buffer */
	if (np->rn_buf) {
		if (np->rn_bufsize > PAGE_SIZE / 2)
			page_free(virt_to_phys(np->rn_buf), np->rn_bufsize,
				  &ramfs_id);
		else
			free(np->rn_buf);
	}

	np->rn_buf = new_buf;
	np->rn_bufsize = new_size;

	return 0;
}

static ssize_t
ramfs_write(file *fp, void *buf, size_t size, off_t offset)
{
	ramfs_node *np = fp->f_vnode->v_data;
	vnode *vp = fp->f_vnode;

	if (!S_ISREG(vp->v_mode) && !S_ISLNK(vp->v_mode))
		return -EINVAL;

	/* Check if the file position exceeds the end of file. */
	if (offset + size > vp->v_size) {
		/* Expand the file size before writing to it */
		const off_t end_pos = offset + size;
		if (ramfs_grow(np, end_pos) < 0)
			return -EIO;

		/* zero sparse file data */
		if (vp->v_size < offset)
			memset(np->rn_buf + vp->v_size, 0, offset - vp->v_size);

		np->rn_size = end_pos;
		vp->v_size = end_pos;
	}
	memcpy(np->rn_buf + offset, buf, size);
	return size;
}

static ssize_t
ramfs_write_iov(file *fp, const iovec *iov, size_t count,
    off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [fp](std::span<std::byte> buf, off_t offset) {
		return ramfs_write(fp, data(buf), size(buf), offset);
	});
}

static int
ramfs_rename(vnode *dvp1, vnode *vp1, vnode *dvp2,
    vnode *vp2, const char *name, size_t name_len)
{
	ramfs_node *np, *old_np;
	int err;

	if (vp2) {
		/* Remove destination file, first */
		err = ramfs_remove_node(dvp2->v_data, vp2->v_data);
		if (err)
			return err;
	}
	/* Same directory ? */
	if (dvp1 == dvp2) {
		/* Change the name of existing file */
		err = ramfs_rename_node(vp1->v_data, name, name_len);
		if (err)
			return err;
	} else {
		/* Create new file or directory */
		old_np = vp1->v_data;
		if (!(np = ramfs_add_node(dvp2->v_data, name, name_len, vp1->v_mode)))
			return -ENOMEM;

		if (S_ISREG(vp1->v_mode)) {
			/* Copy file data */
			np->rn_buf = old_np->rn_buf;
			np->rn_size = old_np->rn_size;
			np->rn_bufsize = old_np->rn_bufsize;
		}
		/* Remove source file */
		ramfs_remove_node(dvp1->v_data, vp1->v_data);
	}
	return 0;
}

/*
 * @vp: vnode of the directory.
 */
static int
ramfs_readdir(file *fp, dirent *buf, size_t len)
{
	size_t remain = len;
	ramfs_node *dnp = fp->f_vnode->v_data;
	ramfs_node *np;

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

	np = dnp->rn_child;
	if (np == NULL)
		goto out;

	for (off_t i = 0; i != (fp->f_offset - 2); i++) {
		np = np->rn_next;
		if (np == NULL)
			goto out;
	}

	while (np != NULL) {
		if (dirbuf_add(&buf, &remain, 0, fp->f_offset,
		    IFTODT(np->rn_mode), np->rn_name))
			goto out;
		++fp->f_offset;
		np = np->rn_next;
	}

out:
	if (remain != len)
		return len - remain;

	return -ENOENT;
}

