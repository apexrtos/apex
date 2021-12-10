#pragma once

/*
 * syscalls.h - syscall declarations
 *
 * Note: this file must compile as C.
 */

#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>

struct dirent;
struct iovec;
struct k_itimerval;
struct k_sigaction;
struct k_sigset_t;
struct rusage;
struct sched_param;
struct stat;
struct statfs;
struct statx;
struct timespec32;
struct timespec;
struct utsname;

#if defined(__cplusplus)
extern "C" {
#endif

void sc_exit(void);
void sc_exit_group(int);
int sc_set_tid_address(int *);
int sc_uname(struct utsname *);
int sc_reboot(unsigned long, unsigned long, int, void *);
int sc_nanosleep(const struct timespec32 *, struct timespec32 *);
int sc_clock_gettime(clockid_t, struct timespec *);
int sc_clock_settime(clockid_t, const struct timespec *);
int sc_clock_settime32(clockid_t, const struct timespec32 *);
int sc_gettid(void);
int sc_sched_getparam(int, struct sched_param *);
int sc_sched_getscheduler(int);
int sc_sched_setscheduler(int, int, const struct sched_param *);
int sc_getitimer(int, struct k_itimerval *);
int sc_setitimer(int, const struct k_itimerval *, struct k_itimerval *);
int sc_access(const char *, int);
int sc_chdir(const char*);
int sc_chmod(const char *, mode_t);
int sc_chown(const char *, uid_t, gid_t);
int sc_faccessat(int, const char *, int, int);
int sc_fchmodat(int, const char *, mode_t, int);
int sc_fchownat(int, const char *, uid_t, gid_t, int);
int sc_fcntl(int, int, void *);
int sc_fstat(int, struct stat *);
int sc_fstatat(int, const char *, struct stat *, int);
int sc_fstatfs(int, size_t, struct statfs *);
int sc_getcwd(char *, size_t);
int sc_getdents(int, struct dirent *, size_t);
int sc_ioctl(int, int, void *);
int sc_lchown(const char *, uid_t, gid_t);
int sc_lstat(const char *, struct stat *);
int sc_llseek(int, long, long, off_t *, int);
int sc_mkdir(const char *, mode_t);
int sc_mkdirat(int, const char*, mode_t);
int sc_mknod(const char *, mode_t, dev_t);
int sc_mknodat(int, const char *, mode_t, dev_t);
int sc_mount(const char *, const char *, const char *, unsigned long, const void *);
int sc_open(const char *, int, int);
int sc_openat(int, const char*, int, int);
int sc_pipe(int [2]);
int sc_pipe2(int [2], int);
int sc_rename(const char*, const char*);
int sc_renameat(int, const char*, int, const char*);
int sc_rmdir(const char*);
int sc_stat(const char *, struct stat *);
int sc_statfs(const char *, size_t, struct statfs *);
int sc_statx(int, const char *, int, unsigned, struct statx *);
int sc_symlink(const char*, const char*);
int sc_symlinkat(const char*, int, const char*);
int sc_sync(void);
int sc_umount2(const char *, int);
int sc_unlink(const char *);
int sc_unlinkat(int, const char *, int);
int sc_utimensat(int, const char *, const struct timespec[2], int);
ssize_t sc_pread(int, void *, size_t, off_t);
ssize_t sc_pwrite(int, const void *, size_t, off_t);
ssize_t sc_read(int, void *, size_t);
ssize_t sc_readlink(const char *, char *, size_t);
ssize_t sc_readlinkat(int, const char *, char *, size_t);
ssize_t sc_readv(int, const struct iovec *, int);
ssize_t sc_write(int, const void *, size_t);
ssize_t sc_writev(int, const struct iovec *, int);
long sc_mmap2(void *, size_t, int, int, int, int);
int sc_munmap(void *, size_t);
int sc_mprotect(void *, size_t, int);
int sc_madvise(void *, size_t, int);
long sc_brk(void *);
int sc_clone(unsigned long, void *, void *, unsigned long, void *);
int sc_fork(void);
int sc_vfork(void);
int sc_execve(const char *, const char *const[], const char *const[]);
int sc_futex(int *, int, int, void *, int *);
int sc_rt_sigaction(int, const struct k_sigaction *, struct k_sigaction *, size_t);
int sc_rt_sigprocmask(int, const struct k_sigset_t *, struct k_sigset_t *, size_t);
int sc_rt_sigreturn(void);
int sc_sigreturn(void);
int sc_syslog(int, char *, int);
pid_t sc_wait4(pid_t, int *, int, struct rusage *);
int sc_waitid(idtype_t, id_t, siginfo_t *, int, struct rusage *);
int sc_tkill(int, int);
int sc_tgkill(pid_t, int, int);

#if UINTPTR_MAX == 0xffffffff
ssize_t sc_preadv(int, const struct iovec *, int, long, long);
ssize_t sc_pwritev(int, const struct iovec *, int, long, long);
#else
ssize_t sc_preadv(int, const struct iovec *, int, off_t);
ssize_t sc_pwritev(int, const struct iovec *, int, off_t);
#endif

#if defined(__cplusplus)
}
#endif
