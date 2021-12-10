/*
 * vfs.c - file system routines
 *
 * All routines return negative error codes and >= 0 for success.
 */

#include "vfs.h"
#include <fs.h>

#include "debug.h"
#include "file.h"
#include "mount.h"
#include "pipe.h"
#include "util.h"
#include "vnode.h"
#include <alloca.h>
#include <arch/interrupt.h>
#include <cassert>
#include <climits>
#include <compiler.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <kernel.h>
#include <page.h>
#include <sch.h>
#include <sig.h>
#include <sync.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <syscalls.h>
#include <task.h>
#include <thread.h>
#include <unistd.h>

/*
 * Page ownership identifier for VFS
 */
static char vfs_id;

/*
 * Semaphore for cleaning up zombie tasks
 */
static semaphore exit_sem;

/*
 * Flags on file member of task (low 2 bits of pointer)
 */
#define FF_CLOEXEC 1

/*
 * File pointer value for reserved fd slot
 */
#define FP_RESERVED ((uintptr_t)-1)

/*
 * unlock task file system lock
 */
static void
task_read_unlock(task *t)
{
	rwlock_read_unlock(&t->fs_lock);
}

static void
task_write_unlock(task *t)
{
	rwlock_write_unlock(&t->fs_lock);
}

/*
 * lock task file system lock
 */
static int
task_read_lock_interruptible(task *t)
{
	return rwlock_read_lock_interruptible(&t->fs_lock);
}

static int
task_write_lock_interruptible(task *t)
{
	return rwlock_write_lock_interruptible(&t->fs_lock);
}

/*
 * lock task file system lock
 */
static void
task_read_lock(task *t)
{
	rwlock_read_lock(&t->fs_lock);
}

static void
task_write_lock(task *t)
{
	rwlock_write_lock(&t->fs_lock);
}

/*
 * fp_ptr - get file pointer from stored file
 */
static file *
fp_ptr(uintptr_t f)
{
	if (f == FP_RESERVED)
		return nullptr;
	return (file *)(f & -4);
}

/*
 * fp_flags - get fd flags from stored file
 */
static int
fp_flags(uintptr_t f)
{
	return f & 3;
}

/*
 * task_getfp_unlocked - Get file pointer from task/fd pair without locking
 *			 underlying vnode.
 *
 * returns NULL if fd is invalid, valid file pointer otherwise.
 */
static file *
task_getfp_unlocked(task *t, int fd)
{
	assert(rwlock_locked(&t->fs_lock));

	file *fp;

	if (fd == AT_FDCWD)
		fp = t->cwdfp;
	else if ((size_t)fd >= ARRAY_SIZE(t->file))
		return nullptr;
	else if (!(fp = fp_ptr(t->file[fd])))
		return nullptr;

	return fp;
}

/*
 * task_getfp - Get file pointer from task/fd pair and lock underlying vnode.
 *
 * returns NULL if fd is invalid, valid file pointer otherwise.
 */
static file *
task_getfp(task *t, int fd)
{
	file *fp;
	if (!(fp = task_getfp_unlocked(t, fd)))
		return nullptr;
	vn_lock(fp->f_vnode);
	++fp->f_count;
	return fp;
}

/*
 * task_getfp_interruptible - Interruptible version of task_getfp
 *
 * returns -ve error code on failure, valid file pointer otherwise.
 */
static expect<file *>
task_getfp_interruptible(task *t, int fd)
{
	int err;
	file *fp;
	if (!(fp = task_getfp_unlocked(t, fd)))
		return DERR(std::errc::bad_file_descriptor);
	if ((err = vn_lock_interruptible(fp->f_vnode)))
		return std::errc{-err};
	++fp->f_count;
	return fp;
}

/*
 * task_file - Get file pointer from task/fd pair with task locking.
 *
 * returns NULL if fd is invalid, valid file pointer otherwise.
 */
static file *
task_file(task *t, int fd)
{
	task_read_lock(t);
	file *fp = task_getfp(t, fd);
	task_read_unlock(t);
	return fp;
}

/*
 * task_file_interruptible - interruptible version of task_file.
 *
 * returns -ve error code on failure, valid file pointer otherwise.
 */
static expect<file *>
task_file_interruptible(task *t, int fd)
{
	int err;
	if ((err = task_read_lock_interruptible(t)))
		return std::errc{-err};
	auto fp = task_getfp_interruptible(t, fd);
	task_read_unlock(t);
	return fp;
}

/*
 * task_newfp - Allocate new file descriptor in the task.
 * Find the smallest empty slot in the fd array.
 * Returns -1 if there is no empty slot.
 * Must be called with task locked.
 */
static int
task_newfd(task *t, size_t start)
{
	assert(rwlock_write_locked(&t->fs_lock));
	assert(start < ARRAY_SIZE(t->file));

	size_t fd;

	for (fd = start; fd < ARRAY_SIZE(t->file); ++fd)
		if (!t->file[fd])
			break;
	if (fd == ARRAY_SIZE(t->file))
		return -1;	/* slot full */

	return fd;
}

/*
 * flags_allow_write - Check if file flags allow write
 */
static bool
flags_allow_write(int flags)
{
	return (flags & O_WRONLY) || (flags & O_RDWR);
}

/*
 * flags_allow_read - Check if file flags allow read
 */
static bool
flags_allow_read(int flags)
{
	return !(flags & O_WRONLY);
}

/*
 * mount_readonly - Check if vnode mount is read only
 */
static bool
mount_readonly(const vnode *vp)
{
	return vp->v_mount->m_flags & MS_RDONLY;
}

/*
 * lookup_v - Lookup path relative to vnode vp
 *
 * Returns locked, referenced vnode in *vpp on success.
 * Always calls vput(vp)
 *
 * Errors:				*vpp		    *node
 * 0:		node exists		refed/locked node   *
 * -ENOENT:	node does not exist	refed/locked dir    remainder of path
 * -ENOTDIR:	vp is not a directory	*		    *
 * -E*:					*		    *
 */
static int
lookup_v(vnode *vp, const char *path, vnode **vpp,
    const char **node, size_t *node_len, int flags, size_t linkcount)
{
	int err = 0;
	size_t len;
	page_ptr p;
	char *link_buf = 0;
	size_t link_buf_size = 0;

	assert(mutex_owner(&vp->v_lock) == thread_cur());

	vdbgvn("lookup_v: vp=%p path=%s vpp=%p node=%p\n",
	    vp, path, vpp, node);

	if (!path || !*path) {
		err = DERR(-EINVAL);
		goto out;
	}

	if (*path == '/' && vp->v_parent) {
		/* absolute path */
		vput(vp);
		if (!(vp = vn_lookup(nullptr, "", 0)))
			return DERR(-EIO);
		++path;
	}

	/*
	 * To avoid deadlocks we must always lock parent nodes before child nodes.
	 * We must not hold a child node lock while locking a parent node.
	 */
	while (*path || (S_ISLNK(vp->v_mode) && !(flags & O_NOFOLLOW))) {
		vnode *child;

		assert(mutex_owner(&vp->v_lock) == thread_cur());

		vdbgvn("lookup_v(trace): vp=%p path=%s\n", vp, path);

		if (S_ISLNK(vp->v_mode)) {
			assert(vp->v_parent);
			const size_t tgt_len = vp->v_size;

			/* detect loops and excessive link depth */
			if (linkcount >= 16) {
				err = DERR(-ELOOP);
				goto out;
			}

			/* bogus link? */
			if (!tgt_len || tgt_len >= PATH_MAX) {
				err = DERR(-EIO);
				goto out;
			}

			/* allocate memory for link target */
			if (tgt_len > link_buf_size) {
				if (tgt_len + 1 > 32) {
					p = page_alloc(PATH_MAX, MA_NORMAL, &vfs_id);
					if (!p) {
						err = DERR(-ENOMEM);
						goto out;
					}
					link_buf = (char *)phys_to_virt(p);
					link_buf_size = PATH_MAX;
				} else {
					link_buf = (char *)alloca(32);
					link_buf_size = 32;
				}
			}

			/* read link target */
			file f = {
				.f_flags = O_RDONLY,
				.f_count = 1,
				.f_offset = 0,
				.f_vnode = vp,
			};
			if ((err = VOP_OPEN(&f, f.f_flags, 0)))
				goto out;
			iovec iov = {
				.iov_base = link_buf,
				.iov_len = link_buf_size,
			};
			err = VOP_READ(&f, &iov, 1, 0);
			VOP_CLOSE(&f);
			if ((size_t)err != tgt_len) {
				VOP_CLOSE(&f);
				if (err >= 0)
					err = DERR(-EIO);
				goto out;
			}
			link_buf[tgt_len] = 0;

			/* lookup relative to parent node */
			vnode *parent = vp->v_parent;
			vref(parent);
			vput(vp);
			vn_lock(parent);
			vp = parent;
			err = lookup_v(vp, link_buf, &vp, 0, 0, 0, ++linkcount);

			if (err == -ENOENT)
				break;
			if (err) {
				vp = 0;
				goto out;
			}
			continue;
		}

		if (!S_ISDIR(vp->v_mode)) {
			err = DERR(-ENOTDIR);
			goto out;
		}

		/* handle "/" */
		if (path[0] == '/') {
			++path;
			continue;
		}

		/* handle "." and "./" */
		if (path[0] == '.' &&
		    (path[1] == '/' || !path[1])) {
			if (!path[1])
				break;
			path += 2;
			continue;
		}

		/* handle ".." and "../" */
		if (path[1] && path[0] == '.' && path[1] == '.' &&
		    (path[2] == '/' || !path[2])) {
			/* ".." from root is still root */
			if (vp->v_parent) {
				vnode *parent = vp->v_parent;
				vref(parent);
				vput(vp);
				vn_lock(parent);
				vp = parent;
			}

			if (!path[2])
				break;
			path += 3;
			continue;
		}

		/* handle "<node>/" and "<node>" */
		len = strchrnul(path, '/') - path;

		if ((child = vn_lookup(vp, path, len))) {
			/* vnode already active */
			vput(vp);
			vp = child;

			path += len;
			continue;
		}

		/* allocate and find child */
		if (!(child = vget(vp->v_mount, vp, path, len))) {
			err = DERR(-ENOMEM);
			goto out;
		}
		if ((err = VOP_LOOKUP(vp, path, len, child))) {
			vput(child);
			if (err == -ENOENT)
				break;
			goto out;
		}
		vput(vp);
		vp = child;

		path += len;
		continue;
	}

	/* only valid if err == -ENOENT */
	if (node)
		*node = path;
	if (node_len)
		*node_len = len;

	*vpp = vp;
	return err;

out:
	if (vp)
		vput(vp);
	return err;
}

/*
 * lookup path on task t relative to directory fd.
 *
 * Returns locked, referenced vnode in *vpp on success.
 *
 * Errors:				*vpp		    *node
 * -EBADF:	fd is not valid		*		    *
 * -ENOTDIR:	fd is not a directory	NULL		    *
 * -ENOENT:	node does not exist	*		    remainder of path
 * -EINVAL:	invalid path		*		    *
 * -ENOMEM:	out of memory		*		    *
 * 0:		file exists		refed/locked node   *
 */
int
lookup_t(task *t, const int fd, const char *path, vnode **vpp,
    const char **node, size_t *node_len, int flags)
{
	int err;
	file *fp;

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	/* lookup_v always calls vput on vp */
	--fp->f_count;
	vref(fp->f_vnode);
	if ((err = lookup_v(fp->f_vnode, path, vpp, node, node_len, flags, 0)) == -ENOENT)
		vput(*vpp);

	return err;
}

/*
 * lookup path on task t relative to directory fd, returning locked directory
 * if path component is missing.
 *
 * Returns locked, referenced vnode in *vpp on success.
 *
 * Errors:				*vpp		    *node
 * -EBADF:	fd is not valid		*		    *
 * -ENOTDIR:	fd is not a directory	NULL		    *
 * -ENOENT:	node does not exist	refed/locked dir    remainder of path
 * -EINVAL:	invalid path		*		    *
 * -ENOMEM:	out of memory		*		    *
 * 0:		file exists		refed/locked node   *
 */
int
lookup_t_dir(task *t, const int fd, const char *path,
    vnode **vpp, const char **node, size_t *node_len, int flags)
{
	file *fp;

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	/* lookup_v always calls vput on vp */
	--fp->f_count;
	vref(fp->f_vnode);
	return lookup_v(fp->f_vnode, path, vpp, node, node_len, flags, 0);
}

/*
 * lookup path on task t relative to directory fd.
 *
 * Returns locked, referenced directory in *vpp on success.
 *
 * Errors:				*vpp		    *node
 * -EBADF:	fd is not valid		*		    *
 * -ENOTDIR:	fd is not a directory	NULL		    *
 * -EEXIST:	node exists		*		    *
 * -ENOENT:	part of path missing	*		    remainder of path
 * -EINVAL:	invalid path		*		    *
 * -ENOMEM:	out of memory		*		    *
 *  0:		node does not exist	refed/locked dir    remainder of path
 */
int
lookup_t_noexist(task *t, const int fd, const char *path,
    vnode **vpp, const char **node, size_t *node_len, int flags)
{
	int err;
	file *fp;
	size_t node_len_;
	const char *node_;

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	/* lookup_v always calls vput on vp */
	--fp->f_count;
	vref(fp->f_vnode);
	err = lookup_v(fp->f_vnode, path, vpp, &node_, &node_len_, flags, 0);

	if (err == 0) {
		err = -EEXIST;
		goto out;
	}

	if (err != -ENOENT)
		goto out;

	if (node_[node_len_] && node_[node_len_ + 1]) {
		/* node is in a missing directory */
		err = DERR(-ENOENT);
		goto out;
	}

	if (node)
		*node = node_;
	if (node_len)
		*node_len = node_len_;

	return 0;

out:
	vput(*vpp);
	return err;
}

/*
 * fs_openfp - file pointer based open
 */
static int
fs_openfp(task *t, const int dirfd, const char *path, int flags,
    mode_t mode, uintptr_t *pfp)
{
	const char *node;
	int err;
	size_t node_len;
	file *fp = nullptr;
	vnode *vp = nullptr;

	vdbgsys("fs_openfp: path=%s flags=%x mode=%x\n", path, flags, mode);

	err = lookup_t_dir(t, dirfd, path, &vp, &node, &node_len, flags);

	/* handle create request */
	if (flags & O_CREAT) {
		if (err == -ENOENT) {
			vnode *nvp;

			/* file doesn't exist */
			if (mount_readonly(vp)) {
				/* can't create on readonly file system */
				err = -EROFS;
				goto out;
			}
			if (node[node_len]) {
				/* node is in a missing directory or is
				 * explicitly a directory */
				err = DERR(-ENOENT);
				goto out;
			}
			/* force mode to regular file */
			mode &= ~S_IFMT;
			mode |= S_IFREG;
			/* try to create */
			if ((err = VOP_MKNOD(vp, node, node_len, flags, mode)))
				goto out;
			/* lookup newly created file */
			if ((err = lookup_v(vp, node, &nvp, nullptr, nullptr, flags, 0))) {
				vput(vp);
				goto out;
			}
			/* replace vp with new vp */
			vp = nvp;
			/* new file is empty */
			flags &= ~O_TRUNC;
		} else if (err) {
			/* lookup failed */
			return err;
		} else {
			/* file already exists */
			if (flags & O_EXCL) {
				/* wanted exclusivity */
				err = DERR(-EEXIST);
				goto out;
			}
			/* need to perform normal file open tests */
			flags &= ~O_CREAT;
		}
	} else if (err) {
		/* file doesn't exist or lookup failure */
		goto out;
	}

	if (!(flags & O_CREAT)) {
		/* opening existing file */
		if (flags_allow_write(flags) || flags & O_TRUNC) {
			/* need write permission */
			if (mount_readonly(vp)) {
				/* on read only file system */
				err = -EROFS;
				goto out;
			}
			if (S_ISDIR(vp->v_mode)) {
				/* on directory */
				err = DERR(-EISDIR);
				goto out;
			}
		}
	}

	if (flags & O_TRUNC) {
		/* try to truncate */
		if ((err = VOP_TRUNCATE(vp)))
			goto out;
	}

	/* create file structure */
	if (*pfp)
		fp = (file *)pfp;
	else if (!(fp = (file *)malloc(sizeof(file))))
			return DERR(-ENOMEM);
	*fp = (file) {
		.f_flags = flags & ~O_CLOEXEC,
		.f_count = 1,
		.f_offset = 0,
		.f_vnode = vp,
	};

	/* try to open */
	if (S_ISFIFO(vp->v_mode))
		err = pipe_open(fp, flags, mode);
	else
		err = VOP_OPEN(fp, flags, mode);
	if (err)
		goto out;

	if (!*pfp)
		*pfp = (uintptr_t)fp | (flags & O_CLOEXEC ? FF_CLOEXEC : 0);
	vn_unlock(vp);
	return 0;

out:
	if (vp)
		vput(vp);
	if (!*pfp)
		free(fp);
	return err;
}

/*
 * vn_reference - increment reference count on vnode
 *
 * for use by vm layer
 */
void
vn_reference(vnode *vn)
{
	vref(vn);
}

/*
 * vn_name - get vnode name
 *
 * for use by vm layer
 */
char *
vn_name(vnode *vn)
{
	return vn->v_name;
}

/*
 * putfp - release reference on file pointer
 */
static int
putfp(file *fp)
{
	int err;
	vnode *vp = fp->f_vnode;

	assert(fp->f_count > 0);
	assert(mutex_owner(&vp->v_lock) == thread_cur());

	vdbgsys("putfp: fp=%x count=%d\n", (u_int)fp, fp->f_count);

	if (--fp->f_count > 0) {
		vn_unlock(fp->f_vnode);
		return 0;
	}

	if (S_ISFIFO(vp->v_mode))
		err = pipe_close(fp);
	else
		err = VOP_CLOSE(fp);

	vput(fp->f_vnode);
	free(fp);
	return err;
}

/*
 * fs_closefp - file pointer based close
 */
static int
fs_closefp(file *fp)
{
	vdbgsys("fs_closefp: fp=%x count=%d\n", (u_int)fp, fp->f_count);
	assert(fp->f_count > 1);
	--fp->f_count;
	return putfp(fp);
}

/*
 * fs_thread - filesystem worker thread
 */
static void
fs_thread(void *arg)
{
	task *t;

	while (true) {
		semaphore_wait_interruptible(&exit_sem);

		/* find next zombie task */
		sch_lock();
		list_for_each_entry(t, &kern_task.link, link) {
			if (t->state != PS_ZOMB)
				continue;
			if (!t->cwdfp)
				continue;
			break;
		}
		sch_unlock();

		if (t == &kern_task)
			continue;

		fs_exit(t);
	}
}

/*
 * fs_init - Initialise data structures and file systems.
 */
void
fs_init()
{
	mount_init();
	vnode_init();
	semaphore_init(&exit_sem);

	kthread_create(&fs_thread, nullptr, PRI_KERN_HIGH, "fs", MA_NORMAL);

	/*
	 * Initialize each file system.
	 */
	extern const vfssw __filesystems, __filesystems_end;
	for (const vfssw *fs = &__filesystems; fs != &__filesystems_end; ++fs) {
		dbg("Initialise %s\n", fs->vs_name);
		fs->vs_op->vfs_init();
	}
}

/*
 * fs_kern_init - Initialise kernel task file system state
 */
void
fs_kinit()
{
	task *t = &kern_task;
	vnode *vp;

	if (!(vp = vn_lookup(nullptr, "", 0)))
		panic("vn_lookup");

	/* create file structure */
	if (!(t->cwdfp = (file *)malloc(sizeof(file))))
		panic("malloc");
	*t->cwdfp = (file) {
		.f_flags = O_RDONLY,
		.f_count = 1,
		.f_offset = 0,
		.f_vnode = vp,
	};

	/* try to open */
	if (VOP_OPEN(t->cwdfp, O_RDONLY, 0))
		panic("open");

	vn_unlock(vp);
}

/*
 * fs_shutdown - Prepare for shutdown
 */
void
fs_shutdown()
{
	sync();
}

/*
 * fs_exit - Called when a task terminates
 *
 * Can be called under interrupt.
 */
void
fs_exit(task *t)
{
	file *fp;
	size_t fd;

	/*
	 * Defer to worker thread if called from an incompatible context
	 *
	 * REVISIT: The sch_locked() part of this is a horrible hack which we
	 * will fix once the rest of the kernel stops leaking locks all over
	 * the place.
	 */
	if (interrupt_running() || sch_locks()) {
		semaphore_post(&exit_sem);
		return;
	}

	task_write_lock(t);

	/*
	 * Block signals as close is an interruptible function. In this context
	 * it is crucial that it runs to completion.
	 */
	const k_sigset_t sig_mask = sig_block_all();

	/*
	 * Close all files opened by task.
	 */
	for (fd = 0; fd < ARRAY_SIZE(t->file); fd++) {
		if ((fp = task_getfp(t, fd))) {
			fs_closefp(fp);
			t->file[fd] = 0;
		}
	}

	/*
	 * Close working directory.
	 */
	if (t->cwdfp) {
		vn_lock(t->cwdfp->f_vnode);
		putfp(t->cwdfp);
		t->cwdfp = 0;
	}

	sig_restore(&sig_mask);
	task_write_unlock(t);
}

/*
 * fs_fork - Called when a new task is forked.
 */
void
fs_fork(task *t)
{
	task *p = task_cur();

	task_read_lock(p);
	task_write_lock(t);

	/* Copy cwd and increment reference count */
	t->cwdfp = p->cwdfp;
	vnode *cwd_vp = t->cwdfp->f_vnode;
	vn_lock(cwd_vp);
	t->cwdfp->f_count++;
	vn_unlock(cwd_vp);

	/* Copy umask */
	t->umask = p->umask;

	/* Inherit file descriptors for all tasks except init */
	if (p == &kern_task)
		goto out;
	for (size_t i = 0; i < ARRAY_SIZE(t->file); i++) {
		file *fp;
		if (!(fp = task_getfp(p, i)))
			continue;
		vnode *vp = fp->f_vnode;
		/* copy FF_CLOEXEC, keep file reference from task_getfp */
		t->file[i] = p->file[i];
		vn_unlock(vp);
	}

out:
	task_write_unlock(t);
	task_read_unlock(p);
}

/*
 * fs_exec - Called when a task calls 'exec'.
 */
void
fs_exec(task *t)
{
	/*
	 * Close directory file descripters and file
	 * descriptors with O_CLOEXEC.
	 */
	task_write_lock(t);
	for (size_t i = 0; i < ARRAY_SIZE(t->file); ++i) {
		file *fp;
		if (!(fp = task_getfp(t, i)))
			continue;
		vnode *vp = fp->f_vnode;
		if (S_ISDIR(vp->v_mode) || fp_flags(t->file[i]) & FF_CLOEXEC) {
			fs_closefp(fp);
			t->file[i] = 0;
			continue;
		}
		putfp(fp);
	}
	task_write_unlock(t);
}

/*
 * open
 */
int
openfor(task *t, int dirfd, const char *path, int flags, ...)
{
	int ret, fd;
	mode_t mode;
	uintptr_t fp = 0;
	file *fpp;
	va_list args;
	int err;

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);

	vdbgsys("openfor: task=%p dirfd=%d path=%s flags=%x mode=%d\n",
	    t, dirfd, path, flags, mode);

	/* reserve slot for file descriptor. */
	if ((err = task_write_lock_interruptible(t)))
		return err;
	if ((fd = task_newfd(t, 0)) < 0) {
		task_write_unlock(t);
		return DERR(-EMFILE);
	}
	t->file[fd] = FP_RESERVED;
	task_write_unlock(t);

	if ((ret = fs_openfp(t, dirfd, path, flags, mode, &fp)) != 0) {
		fp = 0;
		goto out;
	}

	fpp = fp_ptr(fp);

	if (flags & O_NOFOLLOW && S_ISLNK(fpp->f_vnode->v_mode)) {
		vn_lock(fpp->f_vnode);
		putfp(fpp);
		fp = 0;
		ret = -ELOOP;
		goto out;
	}

	ret = fd;

out:
	/* assign fp to reserved slot or unreserve slot in error cases */
	task_write_lock(t);
	t->file[fd] = fp;
	task_write_unlock(t);

	return ret;
}

int
open(const char *path, int flags, ...)
{
	mode_t mode;
	va_list args;

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);

	return openfor(task_cur(), AT_FDCWD, path, flags, mode);
}

int
openat(int dirfd, const char *path, int flags, ...)
{
	mode_t mode;
	va_list args;

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);

	return openfor(task_cur(), dirfd, path, flags, mode);
}

int
kopen(const char *path, int flags, ...)
{
	mode_t mode;
	va_list args;

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);

	return openfor(&kern_task, AT_FDCWD, path, flags, mode);
}

vnode *
vn_open(int fd, int flags)
{
	task *t = task_cur();
	file *fp;

	if (!(fp = task_file(t, fd)))
		return nullptr;

	vnode *vp = fp->f_vnode;

	/* cannot vn_open if filesystem/device requires per-handle data or if
	 * fd was not opened with compatible flags */
	if (fp->f_data || (fp->f_flags & flags) != flags) {
		putfp(fp);
		return nullptr;
	}

	vref(vp);
	putfp(fp);
	return vp;
}

/*
 * utimensat
 */
int
utimensat(int dirfd, const char *path, const timespec *times,
    int flags)
{
	int err;
	vnode *vp;

	vdbgsys("utimensat: dirfd=%d path=%s flags=%x\n",
	    dirfd, path, flags);

	if ((err = lookup_t(task_cur(), dirfd, path, &vp, nullptr, nullptr,
	    flags & AT_SYMLINK_NOFOLLOW ? O_NOFOLLOW : 0)))
		return err;

	/* TODO(time support): implement */

	vput(vp);
	return err;
}

/*
 * close
 */
int
closefor(task *t, int fd)
{
	int err;
	file *fp;

	vdbgsys("closefor: task=%p fd=%d\n", t, fd);

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	task_write_lock(t);
	err = fs_closefp(fp);
	t->file[fd] = 0;
	task_write_unlock(t);

	return err;
}

int
close(int fd)
{
	return closefor(task_cur(), fd);
}

int
kclose(int fd)
{
	return closefor(&kern_task, fd);
}

void
vn_close(vnode *vp)
{
	vn_lock(vp);
	vput(vp);
}

/*
 * mknod
 */
int
mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
{
	const char *node;
	int err;
	size_t node_len;
	vnode *vp;

	vdbgsys("sys_mknod: dirfd=%d path=%s mode=%d dev=%llu\n",
	    dirfd, path, mode, dev);

	switch (mode & S_IFMT) {
	case S_IFREG:
	case S_IFDIR:
	case S_IFIFO:
		/* OK */
		break;
	case S_IFSOCK:
		return DERR(-ENOTSUP);
	default:
		return DERR(-EINVAL);
	}

	if ((err = lookup_t_noexist(task_cur(), dirfd, path, &vp, &node, &node_len, 0)))
		return err;

	if (mount_readonly(vp)) {
		err = -EROFS;
		goto out;
	}

	/* must be dir if name has trailing slash */
	if (node[node_len] && !S_ISDIR(mode)) {
		err = DERR(-ENOENT);
		goto out;
	}

	/* Apex supports a limited set of node types */
	switch (mode & S_IFMT) {
	case S_IFDIR:
	case S_IFREG:
	case S_IFIFO:
	case S_IFLNK:
		err = VOP_MKNOD(vp, node, node_len, 0, mode);
		break;
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
	default:
		err = DERR(-ENOTSUP);
	}

out:
	vput(vp);
	return err;
}

int
mknod(const char *path, mode_t mode, dev_t dev)
{
	return mknodat(AT_FDCWD, path, mode, dev);
}

/*
 * lseek
 *
 * Incorporating Linux compatible extension for Seeking file data and holes.
 *
 * According to Linux Programmer's Manual:
 * "In the simplest implementation, a filesystem can support the
 * operations by making SEEK_HOLE always return the offset of the end
 * of the file, and making SEEK_DATA always return offset (i.e., even
 * if the location referred to by offset is a hole, it can be
 * considered to consist of data that is a sequence of zeros)."
 */
off_t
lseek(int fd, off_t off, int whence)
{
	off_t err;
	file *fp;

	vdbgsys("lseek: fd=%d off=%lld whence=%d\n", fd, off, whence);

	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	vnode *vp = fp->f_vnode;

	if (S_ISFIFO(vp->v_mode)) {
		err = DERR(-ESPIPE);
		goto out;
	}

	/* off > vp->v_size is valid: sparse file */
	off_t x;
	switch (whence) {
	case SEEK_SET:
	case SEEK_DATA:
		x = 0;
		break;
	case SEEK_CUR:
		x = fp->f_offset;
		break;
	case SEEK_END:
		x = vp->v_size;
		break;
	case SEEK_HOLE:
		x = vp->v_size - off;
		break;
	default:
		err = DERR(-EINVAL);
		goto out;
	}

	/* attempt seek to -ve offset */
	if ((x + off) < 0) {
		err = DERR(-EINVAL);
		goto out;
	}

	/* overflow */
	static_assert(sizeof(off_t) == sizeof(int64_t), "bad off_t size");
	if (off > (INT64_MAX - x)) {
		err = DERR(-EOVERFLOW);
		goto out;
	}

	/* set file offset */
	if ((err = VOP_SEEK(fp, x + off, whence)) == 0)
		err = fp->f_offset = x + off;

out:
	putfp(fp);
	return err;
}

/*
 * read
 */
static ssize_t
do_readv(file *fp, const iovec *iov, int count, off_t offset,
    bool update_offset)
{
	ssize_t res;
	vnode *vp = fp->f_vnode;

	vdbgsys("readv: fp=%p iov=%p count=%d offset=%lld\n",
	    fp, iov, count, offset);

	if (!flags_allow_read(fp->f_flags)) {
		/* no read permissions */
		res = DERR(-EPERM);
		goto out;
	}

	switch (IFTODT(vp->v_mode)) {
	case DT_FIFO:
		res = for_each_iov(iov, count, offset,
		    [fp](std::span<std::byte> buf, off_t offset) {
			return pipe_read(fp, data(buf), size(buf), offset);
		});
		break;
	case DT_CHR:
		update_offset = false;
	case DT_BLK:
	case DT_REG:
		res = VOP_READ(fp, iov, count, offset);
		break;
	case DT_DIR:
		res = -EISDIR;
		break;
	case DT_UNKNOWN:
	case DT_LNK:
	case DT_SOCK:
	case DT_WHT:
	default:
		res = -EINVAL;
		break;
	}

	if (update_offset && res > 0)
		fp->f_offset += res;

out:
	putfp(fp);
	return res;
}

ssize_t
read(int fd, void *buf, size_t len)
{
	iovec iov = {
		.iov_base = buf,
		.iov_len = len,
	};

	return readv(fd, &iov, 1);
}

ssize_t
readv(int fd, const iovec *iov, int count)
{
	file *fp;
	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	const bool update_offset = true;
	return do_readv(fp, iov, count, fp->f_offset, update_offset);
}

ssize_t
pread(int fd, void *buf, size_t len, off_t offset)
{
	iovec iov = {
		.iov_base = buf,
		.iov_len = len,
	};

	return preadv(fd, &iov, 1, offset);
}

ssize_t
preadv(int fd, const iovec *iov, int count, off_t offset)
{
	file *fp;
	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	const bool update_offset = false;
	return do_readv(fp, iov, count, offset, update_offset);
}

ssize_t
kpread(int fd, void *buf, size_t len, off_t offset)
{
	iovec iov = {
		.iov_base = buf,
		.iov_len = len,
	};

	return kpreadv(fd, &iov, 1, offset);
}

ssize_t
kpreadv(int fd, const iovec *iov, int count, off_t offset)
{
	file *fp;
	if (auto r = task_file_interruptible(&kern_task, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	const bool update_offset = false;
	return do_readv(fp, iov, count, offset, update_offset);
}

ssize_t
vn_pread(vnode *vp, void *buf, size_t len, off_t offset)
{
	iovec iov = {
		.iov_base = buf,
		.iov_len = len,
	};

	return vn_preadv(vp, &iov, 1, offset);
}

ssize_t
vn_preadv(vnode *vp, const iovec *iov, int count, off_t offset)
{
	/* read from dummy file */
	file f = {
		.f_flags = O_RDONLY,
		.f_count = 99,
		.f_offset = 0,
		.f_data = nullptr,
		.f_vnode = vp,
	};

	vn_lock(vp);

	const bool update_offset = false;
	return do_readv(&f, iov, count, offset, update_offset);
}

/*
 * write
 */
static ssize_t
do_writev(file *fp, const iovec *iov, int count, off_t offset,
    bool update_offset)
{
	ssize_t res;
	vnode *vp = fp->f_vnode;

	/* console driver calls write.. */
	vdbgsys("writev: fp=%p iov=%p count=%d, offset=%lld\n",
		fp, iov, count, offset);

	if (count < 0) {
		res = DERR(-EINVAL);
		goto out;
	}

	if (!flags_allow_write(fp->f_flags)) {
		res = DERR(-EPERM);
		goto out;
	}

	switch (IFTODT(vp->v_mode)) {
	case DT_FIFO:
		res = for_each_iov(iov, count, offset,
		    [fp](std::span<std::byte> buf, off_t offset) {
			return pipe_write(fp, data(buf), size(buf), offset);
		});
		break;
	case DT_CHR:
		update_offset = false;
	case DT_BLK:
	case DT_REG:
		res = VOP_WRITE(fp, iov, count, offset);
		break;
	case DT_DIR:
		res = -EISDIR;
		break;
	case DT_UNKNOWN:
	case DT_LNK:
	case DT_SOCK:
	case DT_WHT:
	default:
		res = -EINVAL;
		break;
	}

	if (update_offset && res > 0)
		fp->f_offset = offset + res;

out:
	putfp(fp);

	return res;
}

ssize_t
write(int fd, const void *buf, size_t len)
{
	if (len == 0)
		return 0;

	iovec iov = {
		.iov_base = (void*)buf,
		.iov_len = len,
	};

	return writev(fd, &iov, 1);
}

ssize_t
writev(int fd, const iovec *iov, int count)
{
	file *fp;
	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	/* append sets file position to end before writing */
	const off_t offset = fp->f_flags & O_APPEND ? fp->f_vnode->v_size
						    : fp->f_offset;

	const bool update_offset = true;
	return do_writev(fp, iov, count, offset, update_offset);
}

ssize_t
pwrite(int fd, const void *buf, size_t len, off_t offset)
{
	if (len == 0)
		return 0;

	iovec iov = {
		.iov_base = (void*)buf,
		.iov_len = len,
	};

	return pwritev(fd, &iov, 1, offset);
}

ssize_t
pwritev(int fd, const iovec *iov, int count, off_t offset)
{
	file *fp;
	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	const bool update_offset = false;
	return do_writev(fp, iov, count, offset, update_offset);
}

ssize_t
kpwrite(int fd, const void *buf, size_t len, off_t offset)
{
	iovec iov = {
		.iov_base = (void*)buf,
		.iov_len = len,
	};

	return kpwritev(fd, &iov, 1, offset);
}

ssize_t
kpwritev(int fd, const iovec *iov, int count, off_t offset)
{
	file *fp;
	if (auto r = task_file_interruptible(&kern_task, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	const bool update_offset = false;
	return do_writev(fp, iov, count, offset, update_offset);
}

/*
 * ioctl
 */
static int
do_ioctl(task *t, int fd, int request, va_list ap)
{
	int err;
	file *fp;
	char *arg;

	arg = va_arg(ap, char *);

	vdbgsys("ioctl: task=%p fd=%d request=%x arg=%p\n",
	    t, fd, request, arg);

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	if (!fp->f_vnode->v_mount)
		err = -ENOSYS; /* pipe */
	else
		err = VOP_IOCTL(fp, request, arg);

	putfp(fp);
	return err;
}

int
ioctl(int fd, int request, ...)
{
	int ret;
	va_list ap;

	va_start(ap, request);
	ret = do_ioctl(task_cur(), fd, request, ap);
	va_end(ap);

	return ret;
}

int
kioctl(int fd, int request, ...)
{
	int ret;
	va_list ap;

	va_start(ap, request);
	ret = do_ioctl(&kern_task, fd, request, ap);
	va_end(ap);

	return ret;
}

/*
 * fsync
 */
int
fsync(int fd)
{
	int err;
	file *fp;

	vdbgsys("fs_fsync: fd=%d\n", fd);

	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	if (!flags_allow_write(fp->f_flags)) {
		putfp(fp);
		return DERR(-EBADF);
	}

	err = VOP_FSYNC(fp);

	putfp(fp);
	return err;
}

/*
 * stat
 */
int
fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	int err;
	vnode *vp;

	vdbgsys("fstatat: dirfd=%d path=%s st=%p flags=%x\n",
	    dirfd, path, st, flags);

	if ((err = lookup_t(task_cur(), dirfd, path, &vp, nullptr, nullptr,
	    flags & AT_SYMLINK_NOFOLLOW ? O_NOFOLLOW : 0)))
		return err;

	err = vn_stat(vp, st);

	vput(vp);
	return err;
}

int
stat(const char *path, struct stat *st)
{
	return fstatat(AT_FDCWD, path, st, 0);
}

static int
do_fstat(task *t, int fd, struct stat *st)
{
	int err;
	file *fp;

	vdbgsys("fstat: task=%p fd=%d st=%p\n", t, fd, st);

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	err = vn_stat(fp->f_vnode, st);

	putfp(fp);
	return err;
}

int
fstat(int fd, struct stat *st)
{
	return do_fstat(task_cur(), fd, st);
}

int
kfstat(int fd, struct stat *st)
{
	return do_fstat(&kern_task, fd, st);
}

/*
 * getdents
 */
int
getdents(int dirfd, dirent *buf, size_t len)
{
	int err;
	file *fp;

	vdbgsys("getdents: dirfd=%d buf=%p len=%zu\n", dirfd, buf, len);

	if (auto r = task_file_interruptible(task_cur(), dirfd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	if (!S_ISDIR(fp->f_vnode->v_mode)) {
		err = DERR(-ENOTDIR);
		goto out;
	}

	err = VOP_READDIR(fp, buf, len);

out:
	putfp(fp);
	return err;
}

/*
 * mkdir
 */
int
mkdirat(int dirfd, const char *path, mode_t mode)
{
	const char *node;
	int err;
	size_t node_len;
	vnode *vp;

	vdbgsys("mkdirat: dirfd=%d path=%s mode=%d\n", dirfd, path, mode);

	if ((err = lookup_t_noexist(task_cur(), dirfd, path, &vp, &node, &node_len, 0)))
		return err;

	if (mount_readonly(vp)) {
		err = -EROFS;
		goto out;
	}

	/* force mode to directory */
	mode &= ~S_IFMT;
	mode |= S_IFDIR;

	err = VOP_MKNOD(vp, node, node_len, 0, mode);

out:
	vput(vp);
	return err;
}

int
mkdir(const char *path, mode_t mode)
{
	return mkdirat(AT_FDCWD, path, mode);
}

/*
 * rmdir
 */
int
rmdir(const char *path)
{
	return unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
}

/*
 * access - check permissions for file access
 */
int
faccessat(int dirfd, const char *path, int mode, int flags)
{
	vnode *vp;
	int err;

	vdbgsys("fs_access: path=%s\n", path);

	if ((err = lookup_t(task_cur(), dirfd, path, &vp, nullptr, nullptr, 0)))
		return err;

	if (((mode & X_OK) && (vp->v_mode & 0111) == 0) ||
	    ((mode & W_OK) && (vp->v_mode & 0222) == 0) ||
	    ((mode & R_OK) && (vp->v_mode & 0444) == 0))
		err = -EACCES;

	vput(vp);
	return err;
}

int
access(const char *path, int mode)
{
	return faccessat(AT_FDCWD, path, mode, 0);
}

/*
 * dup
 */
int
dup(int fildes)
{
	task *t = task_cur();
	file *fp;
	int fildes2;
	vnode *vp;
	int err;

	vdbgsys("dup: fildes=%d\n", fildes);

	if ((size_t)fildes >= ARRAY_SIZE(t->file))
		return DERR(-EBADF);

	if ((err = task_write_lock_interruptible(t)))
		return err;

	if (auto r = task_getfp_interruptible(t, fildes); !r.ok()) {
		task_write_unlock(t);
		return r.sc_rval();
	} else fp = r.val();

	vp = fp->f_vnode;

	/* Find smallest empty slot as new fd. */
	if ((fildes2 = task_newfd(t, 0)) == -1) {
		putfp(fp);
		task_write_unlock(t);
		return DERR(-EMFILE);
	}

	/* don't copy FF_CLOEXEC */
	t->file[fildes2] = (uintptr_t)fp;

	/* keep file reference from task_getfp_interruptible */
	vn_unlock(vp);
	task_write_unlock(t);

	return fildes2;
}

/*
 * dup2
 */
int
dup2for(task *t, int fildes, int fildes2)
{
	file *fp, *fp2;
	int err;

	vdbgsys("dup2for t=%p fildes=%d fildes2=%d\n", t, fildes, fildes2);

	if ((size_t)fildes >= ARRAY_SIZE(t->file) ||
	    (size_t)fildes2 >= ARRAY_SIZE(t->file))
		return DERR(-EBADF);

	if (fildes == fildes2)
		return fildes;

	if ((err = task_write_lock_interruptible(t)))
		return err;

	if (auto r = task_getfp_interruptible(t, fildes); !r.ok()) {
		task_write_unlock(t);
		return r.sc_rval();
	} else fp = r.val();

	if ((fp2 = task_getfp(t, fildes2))) {
		/* Close previous file if it's opened. */
		int err;
		if ((err = fs_closefp(fp2)) != 0) {
			putfp(fp);
			task_write_unlock(t);
			return err;
		}
	}

	/* don't copy FF_CLOEXEC */
	t->file[fildes2] = (uintptr_t)fp;

	/* keep file reference from task_getfp_interruptible */
	vn_unlock(fp->f_vnode);
	task_write_unlock(t);

	return fildes2;
}

int
dup2(int fildes, int fildes2)
{
	return dup2for(task_cur(), fildes, fildes2);
}

int
dup3(int fildes, int fildes2, int flags)
{
	if (flags)
		return DERR(-ENOTSUP);
	return dup2for(task_cur(), fildes, fildes2);
}
/*
 * umask
 */
mode_t
umask(mode_t mask)
{
	task *t = task_cur();
	int err;

	vdbgsys("umask mask=0%03o\n", mask);

	if ((err = task_write_lock_interruptible(t)))
		return err;

	mode_t old = t->umask;
	t->umask = mask;
	task_write_unlock(t);

	return old;
}

/*
 * getcwd
 */
char *
getcwd(char *buf, size_t size)
{
	file *fp;

	vdbgsys("getcwd buf=%p size=%zu\n", buf, size);

	if (!buf)
		return (char*)DERR(-EINVAL);
	if (size < 2)
		return (char*)DERR(-ERANGE);

	if (auto r = task_file_interruptible(task_cur(), AT_FDCWD); !r.ok())
		return (char*)r.err();
	else fp = r.val();

	/* build path from child to parent node */
	char *p = buf + size - 1;
	*p = 0;

	vnode *vp = fp->f_vnode;
	--fp->f_count;
	vref(vp);
	while (vp->v_parent) {
		/* push path component */
		const size_t len = strlen(vp->v_name) + 1;
		p -= len;
		if (p < buf) {
			vput(vp);
			return (char*)DERR(-ERANGE);
		}
		*p = '/';
		memcpy(p + 1, vp->v_name, len - 1);

		/* move to parent */
		vnode *parent = vp->v_parent;
		vref(parent);
		vput(vp);
		vn_lock(parent);
		vp = parent;
	}
	vput(vp);

	if (!*p) {
		/* root directory */
		buf[0] = '/';
		buf[1] = 0;
	} else {
		/* move path into place */
		memmove(buf, p, buf + size - p);
	}

	return buf;
}

/*
 * chdir
 */
int
chdir(const char *path)
{
	task *t = task_cur();
	uintptr_t fp = 0;
	file *fpp;
	int err;

	vdbgsys("chdir path=%s\n", path);

	if ((err = fs_openfp(t, AT_FDCWD, path, 0, O_RDONLY, &fp)))
		return err;
	fpp = fp_ptr(fp);
	if (!S_ISDIR(fpp->f_vnode->v_mode)) {
		vn_lock(fpp->f_vnode);
		putfp(fpp);
		return -ENOTDIR;
	}

	task_write_lock(t);
	vn_lock(t->cwdfp->f_vnode);
	putfp(t->cwdfp);
	t->cwdfp = fpp;
	task_write_unlock(t);

	return 0;
}

/*
 * unlink
 */
int
unlinkat(int dirfd, const char *path, int flags)
{
	int err;
	vnode *vp, *dvp;

	vdbgsys("unlinkat dirfd=%d path=%s flags=%x\n", dirfd, path, flags);

	if ((err = lookup_t(task_cur(), dirfd, path, &vp, nullptr, nullptr, O_NOFOLLOW)))
		return err;

	if (mount_readonly(vp)) {
		vput(vp);
		return -EROFS;
	}

	if (flags & AT_REMOVEDIR) {
		if (!S_ISDIR(vp->v_mode)) {
			vput(vp);
			return -ENOTDIR;
		}
	} else {
		if (S_ISDIR(vp->v_mode)) {
			vput(vp);
			return -EISDIR;
		}
	}

	if (vp->v_flags & VROOT) {
		vput(vp);
		return DERR(-EBUSY);
	}

	/* carefully tap dance to get a lock on vp's parent */
	dvp = vp->v_parent;
	vref(dvp);
	vn_unlock(vp);
	vn_lock(dvp);
	vn_lock(vp);

	if (vp->v_refcnt > 1) {
		vput(vp);
		vput(dvp);
		return DERR(-EBUSY);
	}

	if ((err = VOP_UNLINK(dvp, vp)))
		vput(vp);
	else
		vgone(vp);

	vput(dvp);
	return err;
}

int
unlink(const char *path)
{
	return unlinkat(AT_FDCWD, path, 0);
}

/*
 * fcntl
 */
int
fcntl(int fd, int cmd, ...)
{
	task *t = task_cur();
	file *fp;
	va_list args;
	int arg;
	int err;

	va_start(args, cmd);
	arg = va_arg(args, int);
	va_end(args);

	vdbgsys("fcntl fd=%d cmd=%d arg=%ld\n", fd, cmd, arg);

	if ((err = task_write_lock_interruptible(t)))
		return err;

	if (auto r = task_getfp_interruptible(t, fd); !r.ok()) {
		task_write_unlock(t);
		return r.sc_rval();
	} else fp = r.val();

	int ret = 0;
	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
		if ((size_t)arg >= ARRAY_SIZE(t->file)) {
			ret = DERR(-EINVAL);
			break;
		}

		/* Find empty fd >= arg. */
		if ((ret = task_newfd(t, arg)) == -1) {
			ret = DERR(-EMFILE);
			break;
		}

		/* don't copy FF_CLOEXEC */
		t->file[ret] = (uintptr_t)fp;

		/* set FF_CLOEXEC if requested */
		if (cmd == F_DUPFD_CLOEXEC)
			t->file[ret] |= FF_CLOEXEC;

		/* Increment file reference */
		fp->f_count++;
		break;
	case F_GETFD:
		ret = fp_flags(t->file[fd]) & FF_CLOEXEC ? FD_CLOEXEC : 0;
		break;
	case F_SETFD:
		if (arg & FD_CLOEXEC)
			t->file[fd] |= FF_CLOEXEC;
		else
			t->file[fd] &= ~FF_CLOEXEC;
		break;
	case F_GETFL:
		ret = fp->f_flags;
		break;
	case F_SETFL:
		fp->f_flags = arg;
		break;
	default:
		ret = DERR(-ENOSYS);
		break;
	}
	putfp(fp);
	task_write_unlock(t);
	return ret;
}

/*
 * fstatfs
 */
int
fstatfs(int fd, struct statfs *stf)
{
	task *t = task_cur();
	file *fp;
	vnode *vp;
	int err;

	vdbgsys("fstatfs fd=%d stf=%p\n", fd, stf);

	if (auto r = task_file_interruptible(t, fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	vp = fp->f_vnode;

	if (!vp->v_mount)
		err = DERR(-ENOSYS); /* pipe */
	else
		err = VFS_STATFS(vp->v_mount, stf);

	putfp(fp);
	return err;
}

/*
 * statfs
 */
int
statfs(const char *path, struct statfs *stf)
{
	int err;
	vnode *vp;

	vdbgsys("statfs path=%s stf=%p\n", path, stf);

	if ((err = lookup_t(task_cur(), AT_FDCWD, path, &vp, nullptr, nullptr, 0)))
		return err;

	err = VFS_STATFS(vp->v_mount, stf);

	vput(vp);
	return err;
}

/*
 * pipe
 */
int
pipe2(int fd[2], int flags)
{
	vdbgsys("pipe2 fd=%p flags=%x\n", fd, flags);

	/* TODO: support O_DIRECT */
	if ((flags & (O_CLOEXEC | O_NONBLOCK)) != flags)
		return DERR(-EINVAL);

	task *t = task_cur();
	file *rfp, *wfp;
	vnode *vp;
	int r, rfd, wfd;
	int err;
	const uintptr_t cloexec = flags & O_CLOEXEC ? FF_CLOEXEC : 0;

	if ((err = task_write_lock_interruptible(t)))
		return err;

	/* reserve fd's */
	if ((rfd = task_newfd(t, 0)) < 0) {
		task_write_unlock(t);
		return DERR(-EMFILE);
	}
	t->file[rfd] = FP_RESERVED;
	if ((wfd = task_newfd(t, 0)) < 0) {
		t->file[rfd] = 0;
		task_write_unlock(t);
		return DERR(-EMFILE);
	}
	t->file[wfd] = FP_RESERVED;

	task_write_unlock(t);

	/* create vnode */
	if (!(vp = vget_pipe())) {
		r = DERR(-ENOMEM);
		goto out0;
	}

	/* create file structures */
	if (!(rfp = (file *)malloc(sizeof(file)))) {
		r = DERR(-ENOMEM);
		goto out1;
	}
	if (!(wfp = (file *)malloc(sizeof(file)))) {
		r = DERR(-ENOMEM);
		goto out2;
	}

	*rfp = (file){
		.f_flags = O_RDONLY | (flags & ~O_CLOEXEC),
		.f_count = 1,
		.f_vnode = vp,
	};
	*wfp = (file){
		.f_flags = O_WRONLY | (flags & ~O_CLOEXEC),
		.f_count = 1,
		.f_vnode = vp,
	};

	/* open both ends of pipe */
	if ((r = pipe_open(rfp, rfp->f_flags, 0)) < 0)
		goto out3;
	if ((r = pipe_open(wfp, wfp->f_flags, 0)) < 0)
		goto out3;

	/* we're holding two refs */
	vref(vp);
	vn_unlock(vp);

	fd[0] = rfd;
	fd[1] = wfd;
	r = 0;
	task_write_lock(t);
	t->file[rfd] = (uintptr_t)rfp | cloexec;
	t->file[wfd] = (uintptr_t)wfp | cloexec;
	goto out;

out3:
	free(wfp);
out2:
	free(rfp);
out1:
	vput(vp);
out0:
	task_write_lock(t);
	t->file[rfd] = 0;
	t->file[wfd] = 0;
out:
	task_write_unlock(t);
	return r;
}

int
pipe(int fd[2])
{
	return pipe2(fd, 0);
}

/*
 * symlink
 */
int
symlinkat(const char *target, int dirfd, const char *path)
{
	const char *node;
	int err;
	size_t node_len;
	vnode *dvp;
	file f;
	size_t target_len;

	vdbgsys("symlinkat target=%s dirfd=%d path=%s\n", target, dirfd, path);

	target_len = strnlen(target, PATH_MAX);

	if (!target_len || target_len == PATH_MAX)
		return DERR(-EINVAL);

	if ((err = lookup_t_noexist(task_cur(), dirfd, path, &dvp, &node, &node_len, 0)))
		return err;

	const iovec iov = {
		.iov_base = (void*)target,
		.iov_len = target_len,
	};

	if (mount_readonly(dvp)) {
		err = -EROFS;
		goto out;
	}

	/* path must not have trailing slash */
	if (node[node_len]) {
		err = DERR(-EINVAL);
		goto out;
	}

	/* create node for link */
	if ((err = VOP_MKNOD(dvp, node, node_len, 0, S_IFLNK)))
		goto out;

	/* open link for writing */
	f.f_flags = 1;
	if ((err = fs_openfp(task_cur(), dirfd, path, O_WRONLY | O_NOFOLLOW, 0,
	    (uintptr_t*)&f)))
		goto out;
	if ((err = vn_lock_interruptible(f.f_vnode)))
		goto out1;

	/* write link target */
	if ((size_t)(err = VOP_WRITE(&f, &iov, 1, 0)) != target_len) {
		if (err >= 0)
			err = DERR(-EIO);
		goto out2;
	}

	err = 0;

out2:
	vput(f.f_vnode);
out1:
	VOP_CLOSE(&f);
out:
	vput(dvp);
	return err;
}

int
symlink(const char *target, const char *path)
{
	return symlinkat(target, AT_FDCWD, path);
}

/*
 * readlink
 */
ssize_t
readlinkat(int dirfd, const char *path, char *buf, size_t len)
{
	int res;
	file f;

	vdbgsys("readlinkat dirfd=%d path=%s buf=%p len=%zu\n",
	    dirfd, path, buf, len);

	f.f_flags = 1;
	if ((res = fs_openfp(task_cur(), dirfd, path, O_RDONLY | O_NOFOLLOW, 0,
	    (uintptr_t*)&f)))
		return res;
	if ((res = vn_lock_interruptible(f.f_vnode))) {
		VOP_CLOSE(&f);
		return res;
	}

	iovec iov = {
		.iov_base = buf,
		.iov_len = len,
	};
	res = VOP_READ(&f, &iov, 1, 0);
	if (res >= 0 && res != MIN(f.f_vnode->v_size, len))
		res = DERR(-EIO);

	VOP_CLOSE(&f);
	vput(f.f_vnode);

	return res;
}

ssize_t
readlink(const char *path, char *buf, size_t len)
{
	return readlinkat(AT_FDCWD, path, buf, len);
}

/*
 * rename
 */
int
renameat(int fromdirfd, const char *from, int todirfd, const char *to)
{
	int err;
	vnode *fvp, *tvp;
	vnode *fdvp, *tdvp;
	const char *node;
	size_t node_len;

	vdbgsys("renameat fromdirfd=%d from=%s todirfd=%d to=%s\n",
	    fromdirfd, from, todirfd, to);

	if ((err = lookup_t(task_cur(), fromdirfd, from, &fvp, nullptr, nullptr, O_NOFOLLOW)))
		return err;

	if (fvp->v_mount->m_flags & MS_RDONLY) {
		vput(fvp);
		return -EROFS;
	}

	switch ((err = lookup_t_dir(task_cur(), todirfd, to, &tvp, &node,
	    &node_len, O_NOFOLLOW))) {
	case 0:
		/* target exists, lock & ref parent */
		tdvp = tvp->v_parent;
		vref(tdvp);
		vn_unlock(tvp);
		vn_lock(tdvp);
		vn_lock(tvp);
		node = tvp->v_name;
		node_len = strlen(tvp->v_name);
		break;
	case -ENOENT:
		/* target does not exist */
		tdvp = tvp;
		tvp = 0;
		break;
	default:
		vput(fvp);
		return err;
	}

	/* lock & ref source parent */
	fdvp = fvp->v_parent;
	vref(fdvp);
	vn_unlock(fvp);
	vn_lock(fdvp);
	vn_lock(fvp);

	/* if from == to there's nothing to do */
	if (fvp == tvp) {
		err = 0;
		goto out;
	}

	/* check source & dest are compatible */
	if (tvp) {
		if (S_ISDIR(fvp->v_mode) && !S_ISDIR(tvp->v_mode)) {
			err = -ENOTDIR;
			goto out;
		}
		if (!S_ISDIR(fvp->v_mode) && S_ISDIR(tvp->v_mode)) {
			err = -EISDIR;
			goto out;
		}
	}

	/* check if we are trying to rename into a missing directory */
	if (node[node_len] && node[node_len + 1]) {
		err = -ENOENT;
		goto out;
	}

	/* check if we are trying to rename a file as a directory */
	if (node[node_len] && !S_ISDIR(fvp->v_mode)) {
		err = -ENOTDIR;
		goto out;
	}

	/* this file system doesn't have a proper inode abstraction
	   so this is broken but necessary */
	if (fvp->v_refcnt > 1 || (tvp && tvp->v_refcnt > 1)) {
		err = DERR(-EBUSY);
		goto out;
	}

	/* source & dest must be same file system */
	if (tvp && fvp->v_mount != tvp->v_mount) {
		err = -EXDEV;
		goto out;
	}

	err = VOP_RENAME(fdvp, fvp, tdvp, tvp, node, node_len);

out:
	vput(fvp);
	vput(fdvp);
	if (tvp) vput(tvp);
	vput(tdvp);

	return err;
}

int
rename(const char *from, const char *to)
{
	return renameat(AT_FDCWD, from, AT_FDCWD, to);
}

/*
 * chmod
 */
static int
do_chmod(vnode *vp, mode_t mode)
{
	/* TODO(chmod): implement */

	vput(vp);
	return 0;
}

int
fchmodat(int dirfd, const char *path, mode_t mode, int flags)
{
	int err;
	vnode *vp;

	vdbgsys("fchmodat dirfd=%d path=%s mode=0%03o flags=%x\n",
	    dirfd, path, mode, flags);

	if ((err = lookup_t(task_cur(), dirfd, path, &vp, nullptr, nullptr,
	    flags & AT_SYMLINK_NOFOLLOW ? O_NOFOLLOW : 0)))
		return err;

	return do_chmod(vp, mode);
}

int
fchmod(int fd, mode_t mode)
{
	file *fp;
	vnode *vp;

	vdbgsys("fchmod fd=%d mode=0%03o\n", fd, mode);

	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	vp = fp->f_vnode;

	--fp->f_count;
	vref(vp);
	return do_chmod(vp, mode);
}

int
chmod(const char *path, mode_t mode)
{
	return fchmodat(AT_FDCWD, path, mode, 0);
}

/*
 * chown
 */
static int
do_chown(vnode *vp, uid_t uid, gid_t gid)
{
	/* TODO(chown): implement */

	vput(vp);
	return 0;
}

int
fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags)
{
	int err;
	vnode *vp;

	vdbgsys("fchownat dirfd=%d path=%s uid=%d gid=%d flags=%x\n",
	    dirfd, path, uid, gid, flags);

	if ((err = lookup_t(task_cur(), dirfd, path, &vp, nullptr, nullptr,
	    flags & AT_SYMLINK_NOFOLLOW ? O_NOFOLLOW : 0)))
		return err;

	return do_chown(vp, uid, gid);
}

int
fchown(int fd, uid_t uid, gid_t gid)
{
	file *fp;
	vnode *vp;

	vdbgsys("fchown fd=%d uid=%d gid=%d\n", fd, uid, gid);

	if (auto r = task_file_interruptible(task_cur(), fd); !r.ok())
		return r.sc_rval();
	else fp = r.val();

	vp = fp->f_vnode;

	--fp->f_count;
	vref(vp);
	return do_chown(vp, uid, gid);
}

int
lchown(const char *path, uid_t uid, gid_t gid)
{
	return fchownat(AT_FDCWD, path, uid, gid, AT_SYMLINK_NOFOLLOW);
}

int
chown(const char *path, uid_t uid, gid_t gid)
{
	return fchownat(AT_FDCWD, path, uid, gid, 0);
}

/*
 * file_dump - dump file information
 */
void
file_dump()
{
	list *i;
	task *t;

	info("file dump\n");
	info("=========\n");
	sch_lock();
	i = &kern_task.link;
	do {
		t = list_entry(i, task, link);
		info(" %s (%08x) cwd: %p\n", t->path, (int)t, t->cwdfp->f_vnode);
		info("   fd         fp fp_flags fd_flags count   offset      vnode\n");
		info("  --- ---------- -------- -------- ----- -------- ----------\n");
		for (size_t j = 0; j < ARRAY_SIZE(t->file); ++j) {
			file *f = fp_ptr(t->file[j]);
			if (!f)
				continue;
			int fd_flags = fp_flags(t->file[j]);
			info("  %3d %10p %8x %8x %5d %8ld %10p\n",
			    j, f, f->f_flags, fd_flags, f->f_count,
			    (long)f->f_offset, f->f_vnode);
		}
		i = list_next(i);
	} while (i != &kern_task.link);
	sch_unlock();
}
