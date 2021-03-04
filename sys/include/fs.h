#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

struct dirent;
struct file;
struct iovec;
struct stat;
struct statfs;
struct statx;
struct task;

/*
 * File system management
 */
void	fs_init();
void	fs_kinit();
void	fs_shutdown();

/*
 * These functions perform file system operations on behalf of another task.
 */
void	fs_exit(struct task *);
void	fs_fork(struct task *);
void	fs_exec(struct task *);
int	openfor(struct task *, int, const char *, int, ...);
int	closefor(struct task *, int);
int	dup2for(struct task *, int, int);

/*
 * These functions deal with kernel file handles.
 */
int	kopen(const char *, int, ...);
int	kclose(int);
int	kfstat(int, struct stat *);
ssize_t	kpread(int, void *, size_t, off_t);
ssize_t	kpreadv(int, const struct iovec *, int, off_t);
ssize_t	kpwrite(int, const void *, size_t, off_t);
ssize_t	kpwritev(int, const struct iovec *, int, off_t);
int	kioctl(int, int, ...);

/*
 * These functions deal directly with vnodes.
 */
struct vnode *vn_open(int, int);
void	      vn_reference(struct vnode *);
void	      vn_close(struct vnode *);
ssize_t	      vn_pread(struct vnode *, void *, size_t, off_t);
ssize_t	      vn_preadv(struct vnode *, const struct iovec *, int, off_t);
char	     *vn_name(struct vnode *);

/*
 * Syscalls
 */
int	sc_access(const char *, int);
int	sc_chdir(const char*);
int	sc_chmod(const char *, mode_t);
int	sc_chown(const char *, uid_t, gid_t);
int	sc_faccessat(int, const char *, int, int);
int	sc_fchmodat(int, const char *, mode_t, int);
int	sc_fchownat(int, const char *, uid_t, gid_t, int);
int	sc_fcntl(int, int, void *);
int	sc_fstat(int, struct stat *);
int	sc_fstatat(int, const char *, struct stat *, int);
int	sc_fstatfs(int, size_t, struct statfs *);
int	sc_getcwd(char *, size_t);
int	sc_getdents(int, struct dirent *, size_t);
int	sc_ioctl(int, int, void *);
int	sc_lchown(const char *, uid_t, gid_t);
int	sc_lstat(const char *, struct stat *);
int	sc_llseek(int, long, long, off_t *, int);
int	sc_mkdir(const char *, mode_t);
int	sc_mkdirat(int, const char*, mode_t);
int	sc_mknod(const char *, mode_t, dev_t);
int	sc_mknodat(int, const char *, mode_t, dev_t);
int	sc_mount(const char *, const char *, const char *, unsigned long, const void *);
int	sc_open(const char *, int, int);
int	sc_openat(int, const char*, int, int);
int	sc_pipe(int [2]);
int	sc_pipe2(int [2], int);
int	sc_rename(const char*, const char*);
int	sc_renameat(int, const char*, int, const char*);
int	sc_rmdir(const char*);
int	sc_stat(const char *, struct stat *);
int	sc_statfs(const char *, size_t, struct statfs *);
int	sc_statx(int, const char *, int, unsigned, struct statx *);
int	sc_symlink(const char*, const char*);
int	sc_symlinkat(const char*, int, const char*);
int	sc_sync();
int	sc_umount2(const char *, int);
int	sc_unlink(const char *);
int	sc_unlinkat(int, const char *, int);
int	sc_utimensat(int, const char *, const struct timespec[2], int);
ssize_t	sc_pread(int, void *, size_t, off_t);
ssize_t	sc_pwrite(int, const void *, size_t, off_t);
ssize_t	sc_read(int, void *, size_t);
ssize_t	sc_readlink(const char *, char *, size_t);
ssize_t	sc_readlinkat(int, const char *, char *, size_t);
ssize_t	sc_readv(int, const struct iovec *, int);
ssize_t	sc_write(int, const void *, size_t);
ssize_t	sc_writev(int, const struct iovec *, int);

#if UINTPTR_MAX == 0xffffffff
ssize_t	sc_preadv(int, const struct iovec *, int, long, long);
ssize_t	sc_pwritev(int, const struct iovec *, int, long, long);
#else
ssize_t	sc_preadv(int, const struct iovec *, int, off_t);
ssize_t	sc_pwritev(int, const struct iovec *, int, off_t);
#endif

#include <memory>
extern "C" int close(int fd);
extern "C" int open(const char *pathname, int flags, ...);

namespace std {

template<>
struct default_delete<vnode> {
	void operator()(vnode* v) { vn_close(v); }
};

} /* namespace std */

namespace a {

class fd {
public:
	fd(const char *p, int f) { open(p, f); }
	fd(int fd) : fd_((void*)fd) { }
	operator int() const { return (int)(fd_.get()); }
	void open(const char *p, int f) { fd_.reset((void*)::open(p, f)); }
	void close() { fd_.reset(); }
	int release() { return (int)fd_.release(); }

private:
	struct auto_close {
		void operator()(void *fd) { if ((int)fd > 0) ::close((int)fd); }
	};
	std::unique_ptr<void, auto_close> fd_;
};

} /* namespace a */
