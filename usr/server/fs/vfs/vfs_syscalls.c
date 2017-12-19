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
 * vfs_syscalls.c - everything in this file is a routine implementing
 *                  a VFS system call.
 */

#include <prex/prex.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/list.h>
#include <sys/buf.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "vfs.h"

int
sys_open(char *path, int flags, mode_t mode, file_t *pfp)
{
	vnode_t vp, dvp;
	file_t fp;
	char *filename;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_open: path=%s flags=%x mode=%x\n",
				path, flags, mode));

	flags = FFLAGS(flags);
	if  ((flags & (FREAD | FWRITE)) == 0)
		return EINVAL;
	if (flags & O_CREAT) {
		err = namei(path, &vp);
		if (err == ENOENT) {
			/* Create new file. */
			if ((err = lookup(path, &dvp, &filename)) != 0)
				return err;
			if (dvp->v_mount->m_flags & MNT_RDONLY) {
				vput(dvp);
				return EROFS;
			}
			mode &= ~S_IFMT;
			mode |= S_IFREG;
			err = VOP_CREATE(dvp, filename, mode);
			vput(dvp);
			if (err)
				return err;
			if ((err = namei(path, &vp)) != 0)
				return err;
			flags &= ~O_TRUNC;
		} else if (err) {
			return err;
		} else {
			/* File already exits */
			if (flags & O_EXCL) {
				vput(vp);
				return EEXIST;
			}
			flags &= ~O_CREAT;
		}
	} else {
		/* Open */
		if ((err = namei(path, &vp)) != 0)
			return err;
	}
	if ((flags & O_CREAT) == 0) {
		if (flags & FWRITE || flags & O_TRUNC) {
			if (vp->v_mount->m_flags & MNT_RDONLY) {
				vput(vp);
				return EROFS;
			}
			if (vp->v_type == VDIR) {
				/* Openning directory with writable. */
				vput(vp);
				return EISDIR;
			}
		}
	}
	if (flags & O_TRUNC) {
		if (!(flags & FWRITE) || (vp->v_type != VREG)) {
			vput(vp);
			return EINVAL;
		}
	}
	/* Process truncate request */
	if (flags & O_TRUNC) {
		if ((err = VOP_TRUNCATE(vp)) != 0) {
			vput(vp);
			return err;
		}
	}
	/* Setup file structure */
	if (!(fp = malloc(sizeof(struct file)))) {
		vput(vp);
		return ENOMEM;
	}
	/* Request to file system */
	if ((err = VOP_OPEN(vp, flags)) != 0) {
		free(fp);
		vput(vp);
		return err;
	}
	memset(fp, 0, sizeof(struct file));
	fp->f_vnode = vp;
	fp->f_flags = flags;
	fp->f_offset = 0;
	fp->f_count = 1;
	*pfp = fp;
	vn_unlock(vp);
	return 0;
}

int
sys_close(file_t fp)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_close: fp=%x\n", (u_int)fp));

	vp = fp->f_vnode;
	if (--fp->f_count > 0) {
		vrele(vp);
		return 0;
	}
	vn_lock(vp);
	if ((err = VOP_CLOSE(vp, fp)) != 0) {
		vn_unlock(vp);
		return err;
	}
	vput(vp);
	free(fp);
	return 0;
}

int
sys_read(file_t fp, void *buf, size_t size, size_t *count)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_read: fp=%x buf=%x size=%d\n",
				(u_int)fp, (u_int)buf, size));

	if ((fp->f_flags & FREAD) == 0)
		return EPERM;
	if (size == 0) {
		*count = 0;
		return 0;
	}
	vp = fp->f_vnode;
	vn_lock(vp);
	err = VOP_READ(vp, fp, buf, size, count);
	vn_unlock(vp);
	return err;
}

int
sys_write(file_t fp, void *buf, size_t size, size_t *count)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_write: fp=%x buf=%x size=%d\n",
				(u_int)fp, (u_int)buf, size));

	if (size == 0) {
		*count = 0;
		return 0;
	}
	vp = fp->f_vnode;
	vn_lock(vp);
	err = VOP_WRITE(vp, fp, buf, size, count);
	vn_unlock(vp);
	return err;
}

int
sys_lseek(file_t fp, off_t off, int type, off_t *origin)
{
	vnode_t vp;

	DPRINTF(VFSDB_SYSCALL, ("sys_seek: fp=%x off=%d type=%d\n",
				(u_int)fp, (u_int)off, type));

	vp = fp->f_vnode;
	vn_lock(vp);
	switch (type) {
	case SEEK_SET:
		if (off < 0)
			off = 0;
		if (off > (off_t)vp->v_size)
			off = vp->v_size;
		break;
	case SEEK_CUR:
		if (fp->f_offset + off > (off_t)vp->v_size)
			off = vp->v_size;
		else if (fp->f_offset + off < 0)
			off = 0;
		else
			off = fp->f_offset + off;
		break;
	case SEEK_END:
		if (off > 0)
			off = vp->v_size;
		else if ((int)vp->v_size + off < 0)
			off = 0;
		else
			off = vp->v_size + off;
		break;
	default:
		vn_unlock(vp);
		return EINVAL;
	}
	/* Request to check the file offset */
	if (VOP_SEEK(vp, fp, fp->f_offset, off) != 0) {
		vn_unlock(vp);
		return EINVAL;
	}
	*origin = off;
	fp->f_offset = off;
	vn_unlock(vp);
	return 0;
}

int
sys_ioctl(file_t fp, u_long request, void *buf)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_ioctl: fp=%x request=%x\n", fp, request));

	if ((fp->f_flags & (FREAD | FWRITE)) == 0)
		return EBADF;

	vp = fp->f_vnode;
	vn_lock(vp);
	err = VOP_IOCTL(vp, fp, request, buf);
	vn_unlock(vp);
	return err;
}

int
sys_fsync(file_t fp)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_fsync: fp=%x\n", fp));

	if ((fp->f_flags & FREAD) == 0)
		return EBADF;

	vp = fp->f_vnode;
	vn_lock(vp);
	err = VOP_FSYNC(vp, fp);
	vn_unlock(vp);
	return err;
}

int
sys_fstat(file_t fp, struct stat *st)
{
	vnode_t vp;
	int err = 0;

	DPRINTF(VFSDB_SYSCALL, ("sys_fstat: fp=%x\n", fp));

	vp = fp->f_vnode;
	vn_lock(vp);
	err = vn_stat(vp, st);
	vn_unlock(vp);
	return err;
}

/*
 * Return 0 if directory is empty
 */
static int
check_dir_empty(char *path)
{
	int err;
	file_t fp;
	struct dirent dir;

	if ((err = sys_opendir(path, &fp)) != 0)
		return err;
	do {
		err = sys_readdir(fp, &dir);
		if (err)
			break;
	} while (!strcmp(dir.d_name, ".") || !strcmp(dir.d_name, ".."));

	sys_closedir(fp);

	if (err == ENOENT)
		return 0;
	else if (err == 0)
		return EEXIST;
	return err;
}

int
sys_opendir(char *path, file_t *file)
{
	vnode_t dvp;
	file_t fp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_opendir: path=%s\n", path));

	if ((err = sys_open(path, O_RDONLY, 0, &fp)) != 0)
		return err;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		sys_close(fp);
		return ENOTDIR;
	}
	vn_unlock(dvp);

	*file = fp;
	return 0;
}

int
sys_closedir(file_t fp)
{
	vnode_t dvp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_closedir: fp=%x\n", fp));

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		return EBADF;
	}
	vn_unlock(dvp);
	err = sys_close(fp);
	return err;
}

int
sys_readdir(file_t fp, struct dirent *dir)
{
	vnode_t dvp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_readdir: fp=%x\n", fp));

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		return ENOTDIR;
	}
	err = VOP_READDIR(dvp, fp, dir);
	vn_unlock(dvp);
	return err;
}

int
sys_rewinddir(file_t fp)
{
	vnode_t dvp;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		return EINVAL;
	}
	fp->f_offset = 0;
	vn_unlock(dvp);
	return 0;
}

int
sys_seekdir(file_t fp, long loc)
{
	vnode_t dvp;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		return EINVAL;
	}
	fp->f_offset = (off_t)loc;
	vn_unlock(dvp);
	return 0;
}

int
sys_telldir(file_t fp, long *loc)
{
	vnode_t dvp;

	dvp = fp->f_vnode;
	vn_lock(dvp);
	if (dvp->v_type != VDIR) {
		vn_unlock(dvp);
		return EINVAL;
	}
	*loc = (long)fp->f_offset;
	vn_unlock(dvp);
	return 0;
}

int
sys_mkdir(char *path, mode_t mode)
{
	char *name;
	vnode_t vp, dvp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_mkdir: path=%s mode=%d\n",	path, mode));

	if ((err = namei(path, &vp)) == 0) {
		/* File already exists */
		vput(vp);
		return EEXIST;
	}
	/* Notice: vp is invalid here! */

	if ((err = lookup(path, &dvp, &name)) != 0) {
		/* Directory already exists */
		return err;
	}
	if (dvp->v_mount->m_flags & MNT_RDONLY) {
		err = EROFS;
		goto out;
	}
	mode &= ~S_IFMT;
	mode |= S_IFDIR;

	err = VOP_MKDIR(dvp, name, mode);
 out:
	vput(dvp);
	return err;
}

int
sys_rmdir(char *path)
{
	vnode_t vp, dvp;
	int err;
	char *name;

	DPRINTF(VFSDB_SYSCALL, ("sys_rmdir: path=%s\n", path));

	if ((err = check_dir_empty(path)) != 0)
		return err;
	if ((err = namei(path, &vp)) != 0)
		return err;

	if (vp->v_mount->m_flags & MNT_RDONLY) {
		err = EROFS;
		goto out;
	}
	if (vp->v_type != VDIR) {
		err = ENOTDIR;
		goto out;
	}
	if (vp->v_flags & VROOT || vcount(vp) >= 2) {
		err = EBUSY;
		goto out;
	}
	if ((err = lookup(path, &dvp, &name)) != 0)
		goto out;

	err = VOP_RMDIR(dvp, vp, name);
	vn_unlock(vp);
	vgone(vp);
	vput(dvp);
	return err;

 out:
	vput(vp);
	return err;
}

int
sys_mknod(char *path, mode_t mode)
{
	char *name;
	vnode_t vp, dvp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_mknod: path=%s mode=%d\n",	path, mode));

	switch (mode & S_IFMT) {
	case S_IFREG:
	case S_IFDIR:
	case S_IFIFO:
	case S_IFSOCK:
		/* OK */
		break;
	default:
		return EINVAL;
	}

	if ((err = namei(path, &vp)) == 0) {
		vput(vp);
		return EEXIST;
	}

	if ((err = lookup(path, &dvp, &name)) != 0)
		return err;

	if (dvp->v_mount->m_flags & MNT_RDONLY) {
		err = EROFS;
		goto out;
	}
	if (S_ISDIR(mode))
		err = VOP_MKDIR(dvp, name, mode);
	else
		err = VOP_CREATE(dvp, name, mode);
 out:
	vput(dvp);
	return err;
}

int
sys_rename(char *src, char *dest)
{
	vnode_t vp1, vp2 = 0, dvp1, dvp2;
	char *sname, *dname;
	int err;
	size_t len;
	char root[] = "/";

	DPRINTF(VFSDB_SYSCALL, ("sys_rename: src=%s dest=%s\n", src, dest));

	if ((err = namei(src, &vp1)) != 0)
		return err;
	if (vp1->v_mount->m_flags & MNT_RDONLY) {
		err = EROFS;
		goto err1;
	}
	/* If source and dest are the same, do nothing */
	if (!strncmp(src, dest, PATH_MAX))
		goto err1;

	/* Check if target is directory of source */
	len = strlen(dest);
	if (!strncmp(src, dest, len)) {
		err = EINVAL;
		goto err1;
	}
	/* Is the source busy ? */
	if (vcount(vp1) >= 2) {
		err = EBUSY;
		goto err1;
	}
	/* Check type of source & target */
	err = namei(dest, &vp2);
	if (err == 0) {
		/* target exists */
		if (vp1->v_type == VDIR && vp2->v_type != VDIR) {
			err = ENOTDIR;
			goto err2;
		} else if (vp1->v_type != VDIR && vp2->v_type == VDIR) {
			err = EISDIR;
			goto err2;
		}
		if (vp2->v_type == VDIR && check_dir_empty(dest)) {
			err = EEXIST;
			goto err2;
		}

		if (vcount(vp2) >= 2) {
			err = EBUSY;
			goto err2;
		}
	}

	dname = strrchr(dest, '/');
	if (dname == NULL) {
		err = ENOTDIR;
		goto err2;
	}
	if (dname == dest)
		dest = root;

	*dname = 0;
	dname++;

	if ((err = lookup(src, &dvp1, &sname)) != 0)
		goto err2;

	if ((err = namei(dest, &dvp2)) != 0)
		goto err3;

	/* The source and dest must be same file system */
	if (dvp1->v_mount != dvp2->v_mount) {
		err = EXDEV;
		goto err4;
	}
	err = VOP_RENAME(dvp1, vp1, sname, dvp2, vp2, dname);
 err4:
	vput(dvp2);
 err3:
	vput(dvp1);
 err2:
	if (vp2)
		vput(vp2);
 err1:
	vput(vp1);
	return err;
}

int
sys_unlink(char *path)
{
	char *name;
	vnode_t vp, dvp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_unlink: path=%s\n", path));

	if ((err = namei(path, &vp)) != 0)
		return err;

	if (vp->v_mount->m_flags & MNT_RDONLY) {
		err = EROFS;
		goto out;
	}
	if (vp->v_type == VDIR) {
		err = EPERM;
		goto out;
	}
	if (vp->v_flags & VROOT || vcount(vp) >= 2) {
		err = EBUSY;
		goto out;
	}
	if ((err = lookup(path, &dvp, &name)) != 0)
		goto out;

	err = VOP_REMOVE(dvp, vp, name);

	vn_unlock(vp);
	vgone(vp);
	vput(dvp);
	return 0;
 out:
	vput(vp);
	return err;
}

int
sys_access(char *path, int mode)
{
	vnode_t vp;
	int err;

	DPRINTF(VFSDB_SYSCALL, ("sys_access: path=%s\n", path));

	if ((err = namei(path, &vp)) != 0)
		return err;

	err = EACCES;
	if ((mode & X_OK) && (vp->v_mode & 0111) == 0)
		goto out;
	if ((mode & W_OK) && (vp->v_mode & 0222) == 0)
		goto out;
	if ((mode & R_OK) && (vp->v_mode & 0444) == 0)
		goto out;
	err = 0;
 out:
	vput(vp);
	return err;
}

int
sys_stat(char *path, struct stat *st)
{
	vnode_t vp;
	int err;

	if ((err = namei(path, &vp)) != 0)
		return err;
	err = vn_stat(vp, st);
	vput(vp);
	return err;
}
