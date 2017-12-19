/*-
 * Copyright (c) 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mount.h	8.21 (Berkeley) 5/20/95
 */

#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

#include <sys/cdefs.h>
#include <sys/list.h>
#include <sys/vnode.h>
#include <sys/syslimits.h>

typedef struct { int32_t val[2]; } fsid_t;	/* file system id type */


/*
 * file system statistics
 */
struct statfs {
	short	f_type;			/* filesystem type number */
	short	f_flags;		/* copy of mount flags */
	long	f_bsize;		/* fundamental file system block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	long	f_namelen;		/* maximum filename length */
};

/*
 * Mount data
 */
struct mount {
	struct list	m_link;		/* link to next mount point */
	struct vfsops	*m_op;		/* pointer to vfs operation */
	int		m_flags;	/* mount flag */
	int		m_count;	/* reference count */
	char		m_path[PATH_MAX]; /* mounted path */
	dev_t		m_dev;		/* mounted device */
	struct vnode	*m_root;	/* root vnode */
	struct vnode	*m_covered;	/* vnode covered on parent fs */
	void		*m_data;	/* private data for fs */
};
typedef struct mount *mount_t;


/*
 * Mount flags.
 *
 * Unmount uses MNT_FORCE flag.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* file system written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_UNION	0x00000020	/* union with underlying filesystem */
#define	MNT_ASYNC	0x00000040	/* file system written asynchronously */

/*
 * exported mount flags.
 */
#define	MNT_EXRDONLY	0x00000080	/* exported read only */
#define	MNT_EXPORTED	0x00000100	/* file system is exported */
#define	MNT_DEFEXPORTED	0x00000200	/* exported to the world */
#define	MNT_EXPORTANON	0x00000400	/* use anon uid mapping for everyone */
#define	MNT_EXKERB	0x00000800	/* exported with Kerberos uid mapping */

/*
 * Flags set by internal operations.
 */
#define	MNT_LOCAL	0x00001000	/* filesystem is stored locally */
#define	MNT_QUOTA	0x00002000	/* quotas are enabled on filesystem */
#define	MNT_ROOTFS	0x00004000	/* identifies the root filesystem */

/*
 * Mask of flags that are visible to statfs()
 */
#define	MNT_VISFLAGMASK	0x0000ffff

/*
 * External filesystem control flags.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_DELEXPORT	0x00020000	/* delete export host lists */
#define	MNT_RELOAD	0x00040000	/* reload filesystem data */
#define	MNT_FORCE	0x00080000	/* force unmount or readonly change */
/*
 * Internal filesystem control flags.
 *
 * MNT_UNMOUNT locks the mount entry so that name lookup cannot proceed
 * past the mount point.  This keeps the subtree stable during mounts
 * and unmounts.
 */
#define MNT_UNMOUNT	0x01000000	/* unmount in progress */
#define	MNT_MWAIT	0x02000000	/* waiting for unmount to finish */
#define MNT_WANTRDWR	0x04000000	/* upgrade to read/write requested */

/*
 * Sysctl CTL_VFS definitions.
 *
 * Second level identifier specifies which filesystem. Second level
 * identifier VFS_GENERIC returns information about all filesystems.
 */
#define	VFS_GENERIC		0	/* generic filesystem information */
/*
 * Third level identifiers for VFS_GENERIC are given below; third
 * level identifiers for specific filesystems are given in their
 * mount specific header files.
 */
#define VFS_MAXTYPENUM	1	/* int: highest defined filesystem type */
#define VFS_CONF	2	/* struct: vfsconf for filesystem given
				   as next argument */

/*
 * Flags for various system call interfaces.
 *
 * waitfor flags to vfs_sync() and getfsstat()
 */
#define MNT_WAIT	1
#define MNT_NOWAIT	2


/*
 * Filesystem type switch table.
 */
struct vfssw {
	char		*vs_name;	/* name of file system */
	int		(*vs_init)(void); /* initialize routine */
	struct vfsops	*vs_op;		/* pointer to vfs operation */
};

/*
 * Operations supported on virtual file system.
 */
struct vfsops {
	int (*vfs_mount)	(mount_t mp, char *dev, int flags, void *data);
	int (*vfs_unmount)	(mount_t mp);
	int (*vfs_sync)		(mount_t mp);
	int (*vfs_vget)		(mount_t mp, vnode_t vp);
	int (*vfs_statfs)	(mount_t mp, struct statfs *sfp);
	struct vnops	*vfs_vnops;
};

typedef int (*vfsop_mount_t)(mount_t, char *, int, void *);
typedef int (*vfsop_umount_t)(mount_t);
typedef int (*vfsop_sync_t)(mount_t);
typedef int (*vfsop_vget_t)(mount_t, vnode_t);
typedef int (*vfsop_statfs_t)(mount_t, struct statfs *);

/*
 * VFS interface
 */
#define VFS_MOUNT(MP, DEV, FL, DAT) ((MP)->m_op->vfs_mount)(MP, DEV, FL, DAT)
#define VFS_UNMOUNT(MP)             ((MP)->m_op->vfs_unmount)(MP)
#define VFS_SYNC(MP)                ((MP)->m_op->vfs_sync)(MP)
#define VFS_VGET(MP, VP)            ((MP)->m_op->vfs_vget)(MP, VP)
#define VFS_STATFS(MP, SFP)         ((MP)->m_op->vfs_statfs)(MP, SFP)

#define VFS_NULL		    ((void *)vfs_null)

__BEGIN_DECLS
int	mount(const char *, const char *, const char *, int, void *);
int	umount(const char *);
int	vfs_nullop(void);
int	vfs_einval(void);
__END_DECLS

#endif	/* !_SYS_MOUNT_H */
