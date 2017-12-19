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

#ifndef _VFS_H
#define _VFS_H

#include <sys/cdefs.h>
#include <prex/prex.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/dirent.h>

#include <assert.h>

/*
 * Tunable parameters
 */
#define PRIO_FS		128		/* priority of file system server */
#define FSMAXNAMES	16		/* max length of 'file system' name */

/* #define DEBUG_VFS 1 */

#ifdef DEBUG_VFS
extern int vfs_debug;

#define	VFSDB_CORE	0x00000001
#define	VFSDB_SYSCALL	0x00000002
#define	VFSDB_VNODE	0x00000004
#define	VFSDB_BIO	0x00000008

#define	DPRINTF(_m,X)	if (vfs_debug & (_m)) dprintf X
#else
#define	DPRINTF(_m, X)
#endif

#ifdef DEBUG
#define ASSERT(e)	assert(e)
#else
#define ASSERT(e)
#endif

#if CONFIG_FS_THREADS > 1
#define malloc(s)		malloc_r(s)
#define free(p)			free_r(p)
#else
#define mutex_init(m)		do {} while (0)
#define mutex_destroy(m)	do {} while (0)
#define mutex_lock(m)		do {} while (0)
#define mutex_unlock(m)		do {} while (0)
#define mutex_trylock(m)	do {} while (0)
#endif

/*
 * per task data
 */
struct task {
	struct list	link;		/* hash link */
	task_t		task;		/* task id */
	char 		cwd[PATH_MAX];	/* current working directory */
	file_t		cwdfp;		/* directory for cwd */
	file_t		file[OPEN_MAX];	/* array of file pointers */
	int		nopens;		/* number of opening files */
	mutex_t		lock;		/* lock for this task */
	cap_t		cap;		/* task capabilities */
};

extern const struct vfssw vfssw_table[];

__BEGIN_DECLS
struct task *task_lookup(task_t task);
int	 task_alloc(task_t task, struct task **pt);
void	 task_free(struct task *t);
void	 task_update(struct task *t, task_t task);
void	 task_unlock(struct task *t);
file_t	 task_getfp(struct task *t, int fd);
int	 task_newfd(struct task *t);
int	 task_conv(struct task *t, char *path, char *full);
void	 task_dump(void);
void	 task_init(void);

int	 namei(char *path, vnode_t *vpp);
int	 lookup(char *path, vnode_t *vpp, char **name);
void	 vnode_init(void);
#ifdef DEBUG
void	 vnode_dump(void);
#endif
int	 vfs_findroot(char *path, mount_t *mp, char **root);
void	 vfs_busy(mount_t mp);
void	 vfs_unbusy(mount_t mp);
#ifdef DEBUG
void	 mount_dump(void);
#endif

int	 sys_open(char *path, int flags, mode_t mode, file_t *pfp);
int	 sys_close(file_t fp);
int	 sys_read(file_t fp, void *buf, size_t size, size_t *result);
int	 sys_write(file_t fp, void *buf, size_t size, size_t *result);
int	 sys_lseek(file_t fp, off_t off, int type, off_t * cur_off);
int	 sys_ioctl(file_t fp, u_long request, void *buf);
int	 sys_fstat(file_t fp, struct stat *st);
int	 sys_fsync(file_t fp);

int	 sys_opendir(char *path, file_t * file);
int	 sys_closedir(file_t fp);
int	 sys_readdir(file_t fp, struct dirent *dirent);
int	 sys_rewinddir(file_t fp);
int	 sys_seekdir(file_t fp, long loc);
int	 sys_telldir(file_t fp, long *loc);
int	 sys_mkdir(char *path, mode_t mode);
int	 sys_rmdir(char *path);
int	 sys_mknod(char *path, mode_t mode);
int	 sys_rename(char *src, char *dest);
int	 sys_unlink(char *path);
int	 sys_access(char *path, int mode);
int	 sys_stat(char *path, struct stat *st);

int	 sys_mount(char *dev, char *dir, char *fsname, int flags, void *data);
int	 sys_umount(char *path);
int	 sys_sync(void);
__END_DECLS

#endif /* !_VFS_H */
