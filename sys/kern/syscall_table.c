#include <syscall_table.h>

#include <sched.h>
#include <sections.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <syscall.h>
#include <syscalls.h>
#include <unistd.h>

/* fixup legacy 16-bit junk */
/* copied directly from musl */
#ifdef SYS_getuid32
#undef SYS_lchown
#undef SYS_getuid
#undef SYS_getgid
#undef SYS_geteuid
#undef SYS_getegid
#undef SYS_setreuid
#undef SYS_setregid
#undef SYS_getgroups
#undef SYS_setgroups
#undef SYS_fchown
#undef SYS_setresuid
#undef SYS_getresuid
#undef SYS_setresgid
#undef SYS_getresgid
#undef SYS_chown
#undef SYS_setuid
#undef SYS_setgid
#undef SYS_setfsuid
#undef SYS_setfsgid
#define SYS_lchown SYS_lchown32
#define SYS_getuid SYS_getuid32
#define SYS_getgid SYS_getgid32
#define SYS_geteuid SYS_geteuid32
#define SYS_getegid SYS_getegid32
#define SYS_setreuid SYS_setreuid32
#define SYS_setregid SYS_setregid32
#define SYS_getgroups SYS_getgroups32
#define SYS_setgroups SYS_setgroups32
#define SYS_fchown SYS_fchown32
#define SYS_setresuid SYS_setresuid32
#define SYS_getresuid SYS_getresuid32
#define SYS_setresgid SYS_setresgid32
#define SYS_getresgid SYS_getresgid32
#define SYS_chown SYS_chown32
#define SYS_setuid SYS_setuid32
#define SYS_setgid SYS_setgid32
#define SYS_setfsuid SYS_setfsuid32
#define SYS_setfsgid SYS_setfsgid32
#endif

/*
 * System call table
 *
 * Policy for system calls:
 * - If the system call arguments contain no pointers and the system call
 *   signature matches the C library function signature, call directly through
 *   to a function of the same name. e.g SYS_dup, SYS_dup2.
 * - Otherwise, for SYS_<fn> call a wrapper with the name sc_<fn>.
 * - However, we drop 64/32 suffixes as we don't support linux legacy interfaces.
 *
 * This means that kernel code must not call any function prefixed by sc_.
 *
 * Pointers to read only memory can be verified using either:
 * - u_address -- checks that pointer points to userspace memory
 * - u_access_ok -- checks that the memory region is in userspace and accessible
 * - u_strcheck -- checks that a userspace string is valid
 *
 * Pointers to writable memory must be verified using u_access_ok.
 *
 * However, note that even if access tests succeed a subsequent access could
 * fail if we context switch after the test and another thread unmaps the
 * region. This is OK on MMU systems, but on MPU systems we can't guarantee
 * that the memory won't be remapped by another process or the kernel. So, on
 * NOMMU or MPU systems the address space for the task needs to be locked. This
 * is implemented by u_access_begin/u_access_end.
 *
 * We make use of the MMU/MPU to trap and stub bad accesses to userspace. Bad
 * writes are discarded, bad reads return 0. A fault is marked in the thread
 * state and returned to userspace on syscall return.
 */
__fast_rodata const void *const
syscall_table[SYSCALL_TABLE_SIZE] = {
#ifdef SYS_access
	[SYS_access] = sc_access,
#endif
	[SYS_brk] = sc_brk,
	[SYS_chdir] = sc_chdir,
#ifdef SYS_chmod
	[SYS_chmod] = sc_chmod,				/* stub */
#endif
#ifdef SYS_chown
	[SYS_chown] = sc_chown,
#endif
	[SYS_clock_gettime64] = sc_clock_gettime,
	[SYS_clock_settime64] = sc_clock_settime,
#ifdef SYS_clock_settime32
	[SYS_clock_settime32] = sc_clock_settime32,
#endif
	[SYS_clone] = sc_clone,
	[SYS_close] = close,
	[SYS_dup] = dup,
#ifdef SYS_dup2
	[SYS_dup2] = dup2,
#endif
#ifdef SYS_dup3
	[SYS_dup3] = dup3,
#endif
	[SYS_execve] = sc_execve,
	[SYS_exit] = sc_exit,
	[SYS_exit_group] = sc_exit_group,
	[SYS_faccessat] = sc_faccessat,
	[SYS_fchmod] = fchmod,				/* stub */
	[SYS_fchmodat] = sc_fchmodat,			/* stub */
	[SYS_fchown] = fchown,				/* stub */
	[SYS_fchownat] = sc_fchownat,			/* stub */
	[SYS_fcntl64] = sc_fcntl,
#ifdef SYS_fork
	[SYS_fork] = sc_fork,
#endif
#ifdef SYS_fstat64
	[SYS_fstat64] = sc_fstat,
#endif
#ifdef SYS_fstatat64
	[SYS_fstatat64] = sc_fstatat,
#endif
	[SYS_fstatfs64] = sc_fstatfs,
	[SYS_fsync] = fsync,
	[SYS_futex] = sc_futex,
	[SYS_getcwd] = sc_getcwd,
	[SYS_getdents64] = sc_getdents,
	[SYS_geteuid] = geteuid,
	[SYS_getitimer] = sc_getitimer,
	[SYS_getpgid] = getpgid,
	[SYS_getpid] = getpid,
	[SYS_getppid] = getppid,
	[SYS_getsid] = getsid,
	[SYS_gettid] = sc_gettid,
	[SYS_getuid] = getuid,			/* no user support */
	[SYS_ioctl] = sc_ioctl,
	[SYS_kill] = kill,
#ifdef SYS_lchown
	[SYS_lchown] = sc_lchown,
#endif
#ifdef SYS_lstat64
	[SYS_lstat64] = sc_lstat,
#endif
	[SYS_madvise] = sc_madvise,
#ifdef SYS_mkdir
	[SYS_mkdir] = sc_mkdir,
#endif
	[SYS_mkdirat] = sc_mkdirat,
#ifdef SYS_mknod
	[SYS_mknod] = sc_mknod,
#endif
	[SYS_mknodat] = sc_mknodat,
	[SYS_mmap2] = sc_mmap2,
	[SYS_mount] = sc_mount,
	[SYS_mprotect] = sc_mprotect,
	[SYS_munmap] = sc_munmap,
	[SYS_nanosleep] = sc_nanosleep,
#ifdef SYS_open
	[SYS_open] = sc_open,
#endif
	[SYS_openat] = sc_openat,
	[SYS_pipe2] = sc_pipe2,
#ifdef SYS_pipe
	[SYS_pipe] = sc_pipe,
#endif
	[SYS_prctl] = prctl,
	[SYS_pread64] = sc_pread,
	[SYS_preadv] = sc_preadv,
	[SYS_pwrite64] = sc_pwrite,
	[SYS_pwritev] = sc_pwritev,
	[SYS_read] = sc_read,
#ifdef SYS_readlink
	[SYS_readlink] = sc_readlink,
#endif
	[SYS_readlinkat] = sc_readlinkat,
	[SYS_readv] = sc_readv,
	[SYS_reboot] = sc_reboot,
#ifdef SYS_rename
	[SYS_rename] = sc_rename,
#endif
#ifdef SYS_renameat
	[SYS_renameat] = sc_renameat,
#endif
#ifdef SYS_rmdir
	[SYS_rmdir] = sc_rmdir,
#endif
	[SYS_rt_sigaction] = sc_rt_sigaction,
	[SYS_rt_sigprocmask] = sc_rt_sigprocmask,
	[SYS_rt_sigreturn] = sc_rt_sigreturn,
	[SYS_sched_get_priority_max] = sched_get_priority_max,
	[SYS_sched_get_priority_min] = sched_get_priority_min,
	[SYS_sched_getparam] = sc_sched_getparam,
	[SYS_sched_getscheduler] = sc_sched_getscheduler,
	[SYS_sched_setscheduler] = sc_sched_setscheduler,
	[SYS_sched_yield] = sched_yield,
	[SYS_set_tid_address] = sc_set_tid_address,
	[SYS_setitimer] = sc_setitimer,
	[SYS_setpgid] = setpgid,
	[SYS_setsid] = setsid,
#ifdef SYS_sigreturn
	[SYS_sigreturn] = sc_sigreturn,
#endif
#ifdef SYS_stat64
	[SYS_stat64] = sc_stat,
#endif
	[SYS_statfs64] = sc_statfs,
	[SYS_statx] = sc_statx,			    /* stub */
#ifdef SYS_symlink
	[SYS_symlink] = sc_symlink,
#endif
	[SYS_symlinkat] = sc_symlinkat,
	[SYS_sync] = sc_sync,
	[SYS_syslog] = sc_syslog,
	[SYS_tgkill] = sc_tgkill,
	[SYS_tkill] = sc_tkill,
	[SYS_umask] = umask,
	[SYS_umount2] = sc_umount2,
	[SYS_uname] = sc_uname,
#ifdef SYS_unlink
	[SYS_unlink] = sc_unlink,
#endif
	[SYS_unlinkat] = sc_unlinkat,
#ifdef SYS_utimensat
	[SYS_utimensat] = sc_utimensat,		    /* no time support in FS */
#endif
#ifdef SYS_vfork
	[SYS_vfork] = sc_vfork,
#endif
#ifdef SYS_wait4
	[SYS_wait4] = sc_wait4,
#endif
#ifdef SYS_waitid
	[SYS_waitid] = sc_waitid,
#endif
	[SYS_write] = sc_write,
	[SYS_writev] = sc_writev,
#if UINTPTR_MAX == 0xffffffff
	[SYS__llseek] = sc_llseek,
#else
	[SYS_lseek] = lseek,
#endif
};
