/*
 * fs/syscalls.c - all system calls related to file system operations.
 */
#include <fs.h>

#include <access.h>
#include <arch.h>
#include <debug.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ioctl.h>
#include <kernel.h>
#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/statfs.h>
#include <sys/uio.h>
#include <task.h>
#include <unistd.h>

/*
 * do_iov - copy iov from userspace into kernel, verify all pointers are sane
 * then call through to filesystem routine.
 */
static ssize_t
do_iov(int fd, const struct iovec *uiov, int count, off_t offset,
    ssize_t (*fn)(int fd, const struct iovec *, int, off_t), int prot)
{
	if (!u_address(uiov))
		return DERR(-EFAULT);
	if ((count < 0) || (count > IOV_MAX))
		return DERR(-EINVAL);
	ssize_t ret = 0;
	while (1) {
		struct iovec iov[16];
		size_t l = 0;
		const int c = MIN(count, ARRAY_SIZE(iov));
		for (size_t i = 0; i < c; ++i) {
			iov[i] = uiov[i];
			l += iov[i].iov_len;
			if (!u_access_ok(iov[i].iov_base, l, prot))
				return DERR(-EFAULT);
		}
		const ssize_t r = fn(fd, iov, c, offset);
		if (r == 0)
			return ret;
		if (r < 0) {
			if (ret)
				return ret;
			return r;
		}
		ret += r;
		if (r < l)
			return ret;
		assert(r == l);
		count -= c;
		if (!count)
			return ret;
		uiov += c;
		if (offset >= 0)
			offset += r;
	}
}

/*
 * Syscalls
 */
int
sc_access(const char *path, int mode)
{
	return sc_faccessat(AT_FDCWD, path, mode, 0);
}

int
sc_chdir(const char *path)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return chdir(path);
}

int
sc_chmod(const char *path, mode_t mode)
{
	return sc_fchmodat(AT_FDCWD, path, mode, 0);
}

int
sc_chown(const char *path, uid_t uid, gid_t gid)
{
	return sc_fchownat(AT_FDCWD, path, uid, gid, 0);
}

int
sc_faccessat(int dirfd, const char *path, int mode, int flags)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return faccessat(dirfd, path, mode, flags);
}

int
sc_fchmodat(int dirfd, const char *path, mode_t mode, int flags)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return fchmodat(dirfd, path, mode, flags);
}

int
sc_fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return fchownat(dirfd, path, uid, gid, flags);
}

int
sc_fcntl(int fd, int cmd, void *arg)
{
	if (cmd == F_GETLK &&
	    !u_access_ok(arg, sizeof(struct flock), PROT_WRITE))
		return DERR(-EFAULT);
	return fcntl(fd, cmd, arg);
}

int
sc_fstat(int fd, struct stat *st)
{
	if (!u_access_ok(st, sizeof(*st), PROT_WRITE))
		return DERR(-EFAULT);
	return fstat(fd, st);
}

int
sc_fstatat(int dirfd, const char *path, struct stat *st, int flags)
{
	if (!u_address(path) || !u_access_ok(st, sizeof(*st), PROT_WRITE))
		return DERR(-EFAULT);
	return fstatat(dirfd, path, st, flags);
}

int
sc_fstatfs(int fd, size_t bufsiz, struct statfs *stf)
{
	if (bufsiz != sizeof(*stf))
		return DERR(-EINVAL);
	if (!u_access_ok(stf, sizeof(*stf), PROT_WRITE))
		return DERR(-EFAULT);
	return fstatfs(fd, stf);
}

int
sc_getcwd(char *buf, size_t len)
{
	if (!u_access_ok(buf, len, PROT_WRITE))
		return DERR(-EFAULT);
	char *ret;
	return (ret = getcwd(buf, len)) > (char*)-4096UL ? (int)ret : 1;
}

int
sc_getdents(int dirfd, struct dirent *buf, size_t len)
{
	if (!u_access_ok(buf, len, PROT_WRITE))
		return DERR(-EFAULT);
	return getdents(dirfd, buf, len);
}

int
sc_ioctl(int fd, int request, void *argp)
{
	switch (_IOC_DIR(request)) {
	case _IOC_READ:
		if (!u_access_ok(argp, _IOC_SIZE(request), PROT_WRITE))
			return DERR(-EFAULT);
		break;
	case _IOC_WRITE:
		if (!u_address(argp))
			return DERR(-EFAULT);
		break;
	}
	return ioctl(fd, request, argp);
}

int
sc_lchown(const char *path, uid_t uid, gid_t gid)
{
	return sc_fchownat(AT_FDCWD, path, uid, gid, AT_SYMLINK_NOFOLLOW);
}

int
sc_llseek(int fd, long off0, long off1, off_t *result, int whence)
{
	if (!u_access_ok(result, sizeof(*result), PROT_WRITE))
		return DERR(-EFAULT);
	const off_t r = lseek(fd, (off_t)off0 << 32 | off1, whence);
	if (r < 0)
		return -1;
	*result = r;
	return 0;
}

int
sc_mkdir(const char *path, mode_t mode)
{
	return sc_mkdirat(AT_FDCWD, path, mode);
}

int
sc_mkdirat(int dirfd, const char *path, mode_t mode)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return mkdirat(dirfd, path, mode);
}

int
sc_mknod(const char *path, mode_t mode, dev_t dev)
{
	return sc_mknodat(AT_FDCWD, path, mode, dev);
}

int
sc_mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return mknodat(dirfd, path, mode, dev);
}

int
sc_mount(const char *dev, const char *dir, const char *fs, unsigned long flags,
    const void *data)
{
	if (!u_address(dev) || !u_address(dir) || !u_address(fs) ||
	    (data && !u_address(data)))
		return DERR(-EFAULT);
	if (!task_capable(CAP_ADMIN))
		return DERR(-EPERM);
	return mount(dev, dir, fs, flags, data);
}

int
sc_open(const char *path, int flags, int mode)
{
	return sc_openat(AT_FDCWD, path, flags, mode);
}

int
sc_openat(int dirfd, const char *path, int flags, int mode)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return openat(dirfd, path, flags, mode);
}

int
sc_pipe(int fd[2])
{
	return sc_pipe2(fd, 0);
}

int
sc_pipe2(int fd[2], int flags)
{
	if (!u_access_ok(fd, sizeof(*fd) * 2, PROT_WRITE))
		return DERR(-EFAULT);
	return pipe2(fd, flags);
}

int
sc_rename(const char *from, const char *to)
{
	return sc_renameat(AT_FDCWD, from, AT_FDCWD, to);
}

int
sc_renameat(int fromdirfd, const char *from, int todirfd, const char *to)
{
	if (!u_address(from) || !u_address(to))
		return DERR(-EFAULT);
	return renameat(fromdirfd, from, todirfd, to);
}

int
sc_rmdir(const char *path)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return rmdir(path);
}

int
sc_stat(const char *path, struct stat *st)
{
	if (!u_address(path) || !u_access_ok(st, sizeof(*st), PROT_WRITE))
		return DERR(-EFAULT);
	return stat(path, st);
}

int
sc_statfs(const char *path, size_t bufsiz, struct statfs *stf)
{
	if (bufsiz != sizeof(*stf))
		return DERR(-EINVAL);
	if (!u_address(path) || !u_access_ok(stf, sizeof(*stf), PROT_WRITE))
		return DERR(-EFAULT);
	return statfs(path, stf);
}

int
sc_symlink(const char *target, const char *path)
{
	return sc_symlinkat(target, AT_FDCWD, path);
}

int
sc_symlinkat(const char *target, int dirfd, const char *path)
{
	if (!u_address(target) || !u_address(path))
		return DERR(-EFAULT);
	return symlinkat(target, dirfd, path);
}

int
sc_umount2(const char *dir, int flags)
{
	if (!u_address(dir))
		return DERR(-EFAULT);
	if (!task_capable(CAP_ADMIN))
		return DERR(-EPERM);
	return umount2(dir, flags);
}

int
sc_unlink(const char *path)
{
	return sc_unlinkat(AT_FDCWD, path, 0);
}

int
sc_unlinkat(int dirfd, const char *path, int flags)
{
	if (!u_address(path))
		return DERR(-EFAULT);
	return unlinkat(dirfd, path, flags);
}

int
sc_utimensat(int dirfd, const char *path, const struct timespec times[2],
    int flags)
{
	if (!u_address(path) || !u_access_ok(times, sizeof(*times) * 2, PROT_WRITE))
		return DERR(-EFAULT);
	return utimensat(dirfd, path, times, flags);
}

ssize_t
sc_pread(int fd, void *buf, size_t len, off_t offset)
{
	if (offset < 0)
		return DERR(-EINVAL);
	if (!u_access_ok(buf, len, PROT_WRITE))
		return DERR(-EFAULT);
	return pread(fd, buf, len, offset);
}

ssize_t
sc_pwrite(int fd, const void *buf, size_t len, off_t offset)
{
	if (!u_address(buf))
		return DERR(-EFAULT);
	if (offset < 0)
		return DERR(-EINVAL);
	return pwrite(fd, buf, len, offset);
}

ssize_t
sc_read(int fd, void *buf, size_t len)
{
	if (!u_access_ok(buf, len, PROT_WRITE))
		return DERR(-EFAULT);
	return pread(fd, buf, len, -1);
}

ssize_t
sc_readlink(const char *path, char *buf, size_t len)
{
	return sc_readlinkat(AT_FDCWD, path, buf, len);
}

ssize_t
sc_readlinkat(int dirfd, const char *path, char *buf, size_t len)
{
	if (!u_address(path) || !u_access_ok(buf, len, PROT_WRITE))
		return DERR(-EFAULT);
	return readlinkat(dirfd, path, buf, len);
}

ssize_t
sc_readv(int fd, const struct iovec *iov, int count)
{
	return do_iov(fd, iov, count, -1, preadv, PROT_WRITE);
}

#if UINTPTR_MAX == 0xffffffff
ssize_t
sc_preadv(int fd, const struct iovec *iov, int count, long off1, long off0)
{
	off_t offset = (off_t)off0 << 32 | off1;
#else
ssize_t
sc_preadv(int fd, const struct iovec *iov, int count, off_t offset)
{
#endif
	if (offset < 0)
		return DERR(-EINVAL);
	return do_iov(fd, iov, count, offset, preadv, PROT_WRITE);
}

#if UINTPTR_MAX == 0xffffffff
ssize_t
sc_pwritev(int fd, const struct iovec *iov, int count, long off1, long off0)
{
	off_t offset = (off_t)off0 << 32 | off1;
#else
ssize_t
sc_pwritev(int fd, const struct iovec *iov, int count, off_t offset)
{
#endif
	if (offset < 0)
		return DERR(-EINVAL);
	return do_iov(fd, iov, count, offset, pwritev, PROT_READ);
}

ssize_t
sc_write(int fd, const void *buf, size_t len)
{
	if (!u_access_ok(buf, len, PROT_READ))
		return DERR(-EFAULT);
	return write(fd, buf, len);
}

ssize_t
sc_writev(int fd, const struct iovec *iov, int count)
{
	return do_iov(fd, iov, count, -1, pwritev, PROT_READ);
}
