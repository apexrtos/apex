/*
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

#ifndef _SRV_FS_H
#define _SRV_FS_H

#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <prex/message.h>

#include <limits.h>

/*
 * Messages for file system object
 */
#define FS_MOUNT	0x00000200
#define FS_UMOUNT	0x00000201
#define FS_SYNC		0x00000202
#define FS_OPEN		0x00000203
#define FS_CLOSE	0x00000204
#define FS_MKNOD	0x00000205
#define FS_LSEEK	0x00000206
#define FS_READ		0x00000207
#define FS_WRITE	0x00000208
#define FS_IOCTL	0x00000209
#define FS_FSYNC	0x0000020A
#define FS_FSTAT	0x0000020B
#define FS_OPENDIR	0x0000020C
#define FS_CLOSEDIR	0x0000020D
#define FS_READDIR	0x0000020E
#define FS_REWINDDIR	0x0000020F
#define FS_SEEKDIR	0x00000210
#define FS_TELLDIR	0x00000211
#define FS_MKDIR	0x00000212
#define FS_RMDIR	0x00000213
#define FS_RENAME	0x00000214
#define FS_CHDIR	0x00000215
#define FS_LINK		0x00000216
#define FS_UNLINK	0x00000217
#define FS_STAT		0x00000218
#define FS_GETCWD	0x00000219
#define FS_DUP		0x0000021A
#define FS_DUP2		0x0000021B
#define FS_FCNTL	0x0000021C
#define FS_ACCESS	0x0000021D
#define FS_FORK		0x0000021E
#define FS_EXEC		0x0000021F
#define FS_EXIT		0x00000220
#define FS_REGISTER	0x00000221
#define FS_PIPE		0x00000222

/*
 * Mount message
 */
struct mount_msg {
	struct msg_header hdr;	/* message header */
	char	dev[PATH_MAX];	/* mount device */
	char	dir[PATH_MAX];	/* mount directory */
	char	fs[16];		/* file system name */
	int	flags;		/* mount flags */
	char	data[64];	/* file system specific data */
};

/*
 * File open message
 */
struct open_msg {
	struct msg_header hdr;	/* message header */
	int	flags;		/* open flag */
	mode_t	mode;		/* open mode */
	char	path[PATH_MAX];	/* open file */
	int	fd;		/* file descriptor */
};

/*
 * I/O request message
 */
struct io_msg {
	struct msg_header hdr;	/* message header */
	int	fd;		/* file descriptor */
	char	*buf;		/* i/o buffer */
	size_t	size;		/* read/write size */
};

/*
 * File stat message
 */
struct stat_msg {
	struct msg_header hdr;	/* message header */
	int	fd;		/* file descriptor */
	char	path[PATH_MAX];	/* open file */
	struct stat st;
};

/*
 * Path management message
 */
struct path_msg {
	struct msg_header hdr;	/* message header */
	int	fd;		/* file descriptor */
	char	path[PATH_MAX];
	char	path2[PATH_MAX];
	int	data[4];
};

/*
 * Directory management message
 */
struct dir_msg {
	struct msg_header hdr;	/* message header */
	int	fd;		/* file descriptor */
	struct dirent dirent;	/* directory entry */
};

/*
 * IO cotrol message
 */
struct ioctl_msg {
	struct msg_header hdr;	/* message header */
	int	fd;		/* file descriptor */
	u_long	request;	/* io control command */
	char	buf[IOCPARM_MAX];	/* parameter buffer */
};

/*
 * File control message
 */
struct fcntl_msg {
	struct msg_header hdr;	/* message header */
	int	fd;		/* file descriptor */
	int	cmd;		/* command */
	int	arg;		/* argument */
	struct flock lock;	/* file lock data */
};


#define MAX_FSMSG	sizeof(struct mount_msg)

#endif /* !_SRV_FS_H */
