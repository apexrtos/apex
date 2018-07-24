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

#ifndef vnode_h
#define vnode_h

#include <list.h>
#include <sync.h>

struct dirent;
struct file;
struct iovec;
struct stat;

/*
 * Reading or writing any of these items requires holding the
 * appropriate lock.
 */
struct vnode {
	struct list	 v_link;	/* link for hash map */
	struct mount	*v_mount;	/* mounted vfs pointer */
	struct vnode	*v_parent;	/* pointer to parent vnode */
	unsigned	 v_refcnt;	/* reference count */
	short		 v_flags;	/* vnode flag */
	mode_t		 v_mode;	/* file mode */
	off_t		 v_size;	/* file size */
	struct mutex	 v_lock;	/* lock for this vnode */
	int		 v_blkno;	/* block number */
	char		*v_name;	/* name of node */
	void		*v_data;	/* private data for fs */
	void		*v_pipe;	/* pipe data */
};

/* flags for vnode */
#define VROOT		0x0001		/* root of its file system */
#define VHIDDEN		0x0002		/* vnode hidden */

/*
 * Vnode attribute
 */
struct vattr {
	int		va_type;	/* vnode type */
	mode_t		va_mode;	/* file access mode */
};

/*
 * Modes
 */
#define VREAD		0x0004
#define VWRITE		0x0002
#define VEXEC		0x0001

/*
 * vnode operations
 */
typedef	int (*vnop_open_fn)	(struct file *, int flags, mode_t);
typedef	int (*vnop_close_fn)	(struct file *);
typedef	ssize_t (*vnop_read_fn)	(struct file *, const struct iovec *, size_t);
typedef	ssize_t (*vnop_write_fn)(struct file *, const struct iovec *, size_t);
typedef	int (*vnop_seek_fn)	(struct file *, off_t);
typedef	int (*vnop_ioctl_fn)	(struct file *, u_long, void *);
typedef	int (*vnop_fsync_fn)	(struct file *);
typedef	int (*vnop_readdir_fn)	(struct file *, struct dirent *, size_t);
typedef	int (*vnop_lookup_fn)	(struct vnode *, const char *, size_t, struct vnode *);
typedef	int (*vnop_mknod_fn)	(struct vnode *, const char *, size_t, int flags, mode_t);
typedef	int (*vnop_unlink_fn)	(struct vnode *, struct vnode *);
typedef	int (*vnop_rename_fn)	(struct vnode *, struct vnode *, struct vnode *, struct vnode *, const char *, size_t);
typedef	int (*vnop_getattr_fn)	(struct vnode *, struct vattr *);
typedef	int (*vnop_setattr_fn)	(struct vnode *, struct vattr *);
typedef	int (*vnop_inactive_fn)	(struct vnode *);
typedef	int (*vnop_truncate_fn)	(struct vnode *);

struct vnops {
	vnop_open_fn	    vop_open;
	vnop_close_fn	    vop_close;
	vnop_read_fn	    vop_read;
	vnop_write_fn	    vop_write;
	vnop_seek_fn	    vop_seek;
	vnop_ioctl_fn	    vop_ioctl;
	vnop_fsync_fn	    vop_fsync;
	vnop_readdir_fn	    vop_readdir;
	vnop_lookup_fn	    vop_lookup;
	vnop_mknod_fn	    vop_mknod;
	vnop_unlink_fn	    vop_unlink;
	vnop_rename_fn	    vop_rename;
	vnop_getattr_fn	    vop_getattr;
	vnop_setattr_fn	    vop_setattr;
	vnop_inactive_fn    vop_inactive;
	vnop_truncate_fn    vop_truncate;
};

/*
 * vnode interface
 */
#define VOP_OPEN(FP, F, M)	    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_open)(FP, F, M)
#define VOP_CLOSE(FP)		    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_close)(FP)
#define VOP_READ(FP, I, C)	    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_read)(FP, I, C)
#define VOP_WRITE(FP, I, C)	    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_write)(FP, I, C)
#define VOP_SEEK(FP, OFF)	    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_seek)(FP, OFF)
#define VOP_IOCTL(FP, C, A)	    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_ioctl)(FP, C, A)
#define VOP_FSYNC(FP)		    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_fsync)(FP)
#define VOP_READDIR(FP, B, L)	    ((FP)->f_vnode->v_mount->m_op->vfs_vnops->vop_readdir)(FP, B, L)
#define VOP_LOOKUP(DVP, N, L, VP)   ((DVP)->v_mount->m_op->vfs_vnops->vop_lookup)(DVP, N, L, VP)
#define VOP_MKNOD(DVP, N, L, F, M)  ((DVP)->v_mount->m_op->vfs_vnops->vop_mknod)(DVP, N, L, F, M)
#define VOP_UNLINK(DVP, VP)	    ((DVP)->v_mount->m_op->vfs_vnops->vop_unlink)(DVP, VP)
#define VOP_RENAME(DVP1, VP1, DVP2, VP2, N, L) ((DVP1)->v_mount->m_op->vfs_vnops->vop_rename)(DVP1, VP1, DVP2, VP2, N, L)
#define VOP_GETATTR(VP, VAP)	    ((VP)->v_mount->m_op->vfs_vnops->vop_getattr)(VP, VAP)
#define VOP_SETATTR(VP, VAP)	    ((VP)->v_mount->m_op->vfs_vnops->vop_setattr)(VP, VAP)
#define VOP_INACTIVE(VP)	    ((VP)->v_mount->m_op->vfs_vnops->vop_inactive)(VP)
#define VOP_TRUNCATE(VP)	    ((VP)->v_mount->m_op->vfs_vnops->vop_truncate)(VP)

/*
 * Generic null/invalid operations
 */
int vop_nullop(void);
int vop_einval(void);

/*
 * vnode cache interface
 */
struct vnode	*vget(struct mount *, struct vnode *, const char *, size_t);
struct vnode	*vget_pipe(void);
struct vnode	*vn_lookup(struct vnode *, const char *, size_t);
int		 vn_lock_interruptible(struct vnode *);
void		 vn_lock(struct vnode *);
void		 vn_unlock(struct vnode *);
void		 vn_hide(struct vnode *);
void		 vn_unhide(struct vnode *);
int		 vn_stat(struct vnode *, struct stat *);
void		 vput(struct vnode *);
void		 vref(struct vnode *);
void		 vgone(struct vnode *);

#endif /* !vnode_h */

