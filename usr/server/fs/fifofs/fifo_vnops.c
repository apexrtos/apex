/*
 * Copyright (c) 2008, Kohsuke Ohtani
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
 * fifofs - FIFO/pipe file system.
 */

#include <prex/prex.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/syslog.h>
#include <sys/dirent.h>
#include <sys/list.h>

#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>

#include "fifo.h"

struct fifo_node {
	struct list fn_link;
	char	*fn_name;	/* name (null-terminated) */
	cond_t	fn_rcond;	/* cv for read */
	cond_t	fn_wcond;	/* cv for write */
	mutex_t	fn_rmtx;	/* mutex for read */
	mutex_t	fn_wmtx;	/* mutex for write */
	int	fn_readers;	/* reader count */
	int	fn_writers;	/* writer count */
	int	fn_start;	/* start offset of buffer data */
	int	fn_size;	/* siez of buffer data */
	char	*fn_buf;	/* pointer to buffer */
};

#define fifo_mount	((vfsop_mount_t)vfs_nullop)
#define fifo_unmount	((vfsop_umount_t)vfs_nullop)
#define fifo_sync	((vfsop_sync_t)vfs_nullop)
#define fifo_vget	((vfsop_vget_t)vfs_nullop)
#define fifo_statfs	((vfsop_statfs_t)vfs_nullop)

static int fifo_open	(vnode_t, int);
static int fifo_close	(vnode_t, file_t);
static int fifo_read	(vnode_t, file_t, void *, size_t, size_t *);
static int fifo_write	(vnode_t, file_t, void *, size_t, size_t *);
#define fifo_seek	((vnop_seek_t)vop_nullop)
static int fifo_ioctl	(vnode_t, file_t, u_long, void *);
#define fifo_fsync	((vnop_fsync_t)vop_nullop)
static int fifo_readdir	(vnode_t, file_t, struct dirent *);
static int fifo_lookup	(vnode_t, char *, vnode_t);
static int fifo_create	(vnode_t, char *, mode_t);
static int fifo_remove	(vnode_t, vnode_t, char *);
#define fifo_rename	((vnop_rename_t)vop_einval)
#define fifo_mkdir	((vnop_mkdir_t)vop_einval)
#define fifo_rmdir	((vnop_rmdir_t)vop_einval)
#define fifo_getattr	((vnop_getattr_t)vop_nullop)
#define fifo_setattr	((vnop_setattr_t)vop_nullop)
#define fifo_inactive	((vnop_inactive_t)vop_nullop)
#define fifo_truncate	((vnop_truncate_t)vop_nullop)

static void wait_reader(vnode_t);
static void wakeup_reader(vnode_t);
static void wait_writer(vnode_t);
static void wakeup_writer(vnode_t);

#if CONFIG_FS_THREADS > 1
static mutex_t fifo_lock = MUTEX_INITIALIZER;
#endif

static struct list fifo_head;

/*
 * vnode operations
 */
struct vnops fifofs_vnops = {
	fifo_open,		/* open */
	fifo_close,		/* close */
	fifo_read,		/* read */
	fifo_write,		/* write */
	fifo_seek,		/* seek */
	fifo_ioctl,		/* ioctl */
	fifo_fsync,		/* fsync */
	fifo_readdir,		/* readdir */
	fifo_lookup,		/* lookup */
	fifo_create,		/* create */
	fifo_remove,		/* remove */
	fifo_rename,		/* remame */
	fifo_mkdir,		/* mkdir */
	fifo_rmdir,		/* rmdir */
	fifo_getattr,		/* getattr */
	fifo_setattr,		/* setattr */
	fifo_inactive,		/* inactive */
	fifo_truncate,		/* truncate */
};

/*
 * File system operations
 */
struct vfsops fifofs_vfsops = {
	fifo_mount,		/* mount */
	fifo_unmount,		/* unmount */
	fifo_sync,		/* sync */
	fifo_vget,		/* vget */
	fifo_statfs,		/* statfs */
	&fifofs_vnops,		/* vnops */
};

static int
fifo_open(vnode_t vp, int flags)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("fifo_open: path=%s\n", vp->v_path));

	if (!strcmp(vp->v_path, "/"))	/* root ? */
		return 0;

	/*
	 * Unblock all threads who are waiting in open().
	 */
	if (flags & FREAD) {
		if (np->fn_readers == 0 && np->fn_writers > 0)
			wakeup_writer(vp);
		np->fn_readers++;
	}
	if (flags & FWRITE) {
		if (np->fn_writers == 0 && np->fn_readers > 0)
			wakeup_reader(vp);
		np->fn_writers++;
	}

	/*
	 * If no-one opens FIFO at the other side, wait for open().
	 */
	if (flags & FREAD) {
		if (flags & O_NONBLOCK) {
		} else {
			while (np->fn_writers == 0)
				wait_writer(vp);
		}
	}
	if (flags & FWRITE) {
		if (flags & O_NONBLOCK) {
			if (np->fn_readers == 0)
				return ENXIO;
		} else {
			while (np->fn_readers == 0)
				wait_reader(vp);
		}
	}
	return 0;
}

static int
fifo_close(vnode_t vp, file_t fp)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("fifo_close: fp=%x\n", fp));

	if (np == NULL)
		return 0;

	if (fp->f_flags & FREAD) {
		np->fn_readers--;
		if (np->fn_readers == 0)
			wakeup_writer(vp);
	}
	if (fp->f_flags & FWRITE) {
		np->fn_writers--;
		if (np->fn_writers == 0)
			wakeup_reader(vp);
	}
	if (vp->v_refcnt > 1)
		return 0;

	return 0;
}

static int
fifo_read(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	struct fifo_node *np = vp->v_data;
	char *p = buf;
	int pos, nbytes;

	DPRINTF(("fifo_read\n"));

	/*
	 * If nothing in the pipe, wait.
	 */
	while (np->fn_size == 0) {
		/*
		 * No data and no writer, then EOF
		 */
		if (np->fn_writers == 0) {
			*result = 0;
			return 0;
		}
		/*
 		 * wait for data
		 */
		wait_writer(vp);
	}
	/*
	 * Read
	 */
	nbytes = (np->fn_size < size) ? np->fn_size : size;
	np->fn_size -= nbytes;
	*result = nbytes;
	pos = np->fn_start;
	while (nbytes > 0) {
		*p++ = np->fn_buf[pos];
		if (++pos > PIPE_BUF)
			pos = 0;
		nbytes--;
	}
	np->fn_start = pos;

	wakeup_writer(vp);
	return 0;
}

static int
fifo_write(vnode_t vp, file_t fp, void *buf, size_t size, size_t *result)
{
	struct fifo_node *np = vp->v_data;
	char *p = buf;
	int pos, nfree, nbytes;

	DPRINTF(("fifo_write\n"));

 again:
	/*
	 * If the pipe is full,
	 * wait for reads to deplete
	 * and truncate it.
	 */
	while (np->fn_size >= PIPE_BUF)
		wait_reader(vp);

	/*
	 * Write
	 */
	nfree = PIPE_BUF - np->fn_size;
	nbytes = (nfree < size) ? nfree : size;

	pos = np->fn_start + np->fn_size;
	if (pos >= PIPE_BUF)
		pos -= PIPE_BUF;
	np->fn_size += nbytes;
	size -= nbytes;
	while (nbytes > 0) {
		np->fn_buf[pos] = *p++;
		if (++pos > PIPE_BUF)
			pos = 0;
		nbytes--;
	}

	wakeup_reader(vp);

	if (size > 0)
		goto again;

	*result = size;
	return 0;
}

static int
fifo_ioctl(vnode_t vp, file_t fp, u_long cmd, void *arg)
{
	DPRINTF(("fifo_ioctl\n"));
	return EINVAL;
}

static int
fifo_lookup(vnode_t dvp, char *name, vnode_t vp)
{
	list_t head, n;
	struct fifo_node *np = NULL;
	int found;

	DPRINTF(("fifo_lookup: %s\n", name));

	if (*name == '\0')
		return ENOENT;

	mutex_lock(&fifo_lock);

	found = 0;
	head = &fifo_head;
	for (n = list_first(head); n != head; n = list_next(n)) {
		np = list_entry(n, struct fifo_node, fn_link);
		if (strcmp(name, np->fn_name) == 0) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		mutex_unlock(&fifo_lock);
		return ENOENT;
	}
	vp->v_data = np;
	vp->v_mode = ALLPERMS;
	vp->v_type = VFIFO;
	vp->v_size = 0;

	mutex_unlock(&fifo_lock);
	return 0;
}

static int
fifo_create(vnode_t dvp, char *name, mode_t mode)
{
	struct fifo_node *np;

	DPRINTF(("create %s in %s\n", name, dvp->v_path));

#if 0
	if (!S_ISFIFO(mode))
		return EINVAL;
#endif

	if ((np = malloc(sizeof(struct fifo_node))) == NULL)
		return ENOMEM;

	if ((np->fn_buf = malloc(PIPE_BUF)) == NULL) {
		free(np);
		return ENOMEM;
	}
	np->fn_name = malloc(strlen(name) + 1);
	if (np->fn_name == NULL) {
		free(np->fn_buf);
		free(np);
		return ENOMEM;
	}

	strcpy(np->fn_name, name);
	mutex_init(&np->fn_rmtx);
	mutex_init(&np->fn_wmtx);
	cond_init(&np->fn_rcond);
	cond_init(&np->fn_wcond);
	np->fn_readers = 0;
	np->fn_writers = 0;
	np->fn_start = 0;
	np->fn_size = 0;

	mutex_lock(&fifo_lock);
	list_insert(&fifo_head, &np->fn_link);
	mutex_unlock(&fifo_lock);
	return 0;
}

static int
fifo_remove(vnode_t dvp, vnode_t vp, char *name)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("remove %s in %s\n", name, dvp->v_path));

	mutex_lock(&fifo_lock);
	list_remove(&np->fn_link);
	mutex_unlock(&fifo_lock);

	free(np->fn_name);
	free(np->fn_buf);
	free(np);

	vp->v_data = NULL;
	return 0;
}

/*
 * @vp: vnode of the directory.
 */
static int
fifo_readdir(vnode_t vp, file_t fp, struct dirent *dir)
{
	struct fifo_node *np;
	list_t head, n;
	int i;

	mutex_lock(&fifo_lock);

	if (fp->f_offset == 0) {
		dir->d_type = DT_DIR;
		strcpy((char *)&dir->d_name, ".");
	} else if (fp->f_offset == 1) {
		dir->d_type = DT_DIR;
		strcpy((char *)&dir->d_name, "..");
	} else {
		i = 0;
		np = NULL;
		head = &fifo_head;
		for (n = list_first(head); n != head; n = list_next(n)) {
			if (i == (fp->f_offset - 2)) {
				np = list_entry(n, struct fifo_node, fn_link);
				break;
			}
		}
		if (np == NULL) {
			mutex_unlock(&fifo_lock);
			return ENOENT;
		}
		dir->d_type = DT_FIFO;
		strcpy((char *)&dir->d_name, np->fn_name);
	}
	dir->d_fileno = fp->f_offset;
	dir->d_namlen = strlen(dir->d_name);

	fp->f_offset++;

	mutex_unlock(&fifo_lock);
	return 0;
}

int
fifofs_init(void)
{
	list_init(&fifo_head);
	return 0;
}


static void
wait_reader(vnode_t vp)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("wait_reader: %x\n", np));
	vn_unlock(vp);
	mutex_lock(&np->fn_rmtx);
	cond_wait(&np->fn_rcond, &np->fn_rmtx);
	mutex_unlock(&np->fn_rmtx);
	vn_lock(vp);
}

static void
wakeup_writer(vnode_t vp)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("wakeup_writer: %x\n", np));
	cond_broadcast(&np->fn_rcond);
}

static void
wait_writer(vnode_t vp)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("wait_writer: %x\n", np));
	vn_unlock(vp);
	mutex_lock(&np->fn_wmtx);
	cond_wait(&np->fn_wcond, &np->fn_wmtx);
	mutex_unlock(&np->fn_wmtx);
	vn_lock(vp);
}

static void
wakeup_reader(vnode_t vp)
{
	struct fifo_node *np = vp->v_data;

	DPRINTF(("wakeup_reader: %x\n", np));
	cond_broadcast(&np->fn_wcond);
}
