#pragma once

#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct iovec;
struct task;
struct vnode;

/*
 * File system management
 */
void fs_init();
void fs_kinit();
void fs_shutdown();

/*
 * These functions perform file system operations on behalf of another task.
 */
void fs_exit(task *);
void fs_fork(task *);
void fs_exec(task *);
int openfor(task *, int, const char *, int, ...);
int closefor(task *, int);
int dup2for(task *, int, int);

/*
 * These functions deal with kernel file handles.
 */
int kopen(const char *, int, ...);
int kclose(int);
int kfstat(int, struct stat *);
ssize_t kpread(int, void *, size_t, off_t);
ssize_t kpreadv(int, const iovec *, int, off_t);
ssize_t kpwrite(int, const void *, size_t, off_t);
ssize_t kpwritev(int, const iovec *, int, off_t);
int kioctl(int, int, ...);

/*
 * These functions deal directly with vnodes.
 */
vnode *vn_open(int, int);
void vn_reference(vnode *);
void vn_close(vnode *);
ssize_t vn_pread(vnode *, void *, size_t, off_t);
ssize_t vn_preadv(vnode *, const iovec *, int, off_t);
char *vn_name(vnode *);

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
