#include <access.h>
#include <arch.h>
#include <assert.h>
#include <clone.h>
#include <debug.h>
#include <errno.h>
#include <exec.h>
#include <fcntl.h>
#include <fs.h>
#include <futex.h>
#include <kernel.h>
#include <mmap.h>
#include <proc.h>
#include <sch.h>
#include <sched.h>
#include <sections.h>
#include <sig.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <syscall.h>
#include <syscall_table.h>
#include <task.h>
#include <thread.h>
#include <time.h>
#include <unistd.h>
#include <version.h>

/*
 * Some system call wrappers
 */
static void
sc_exit(int status)
{
	thread_terminate(thread_cur());
}

static void
sc_exit_group(int status)
{
	proc_exit(task_cur(), status);
}

static int
sc_set_tid_address(void *p)
{
	if (!u_address(p))
		return DERR(-EFAULT);
	thread_cur()->clear_child_tid = p;
	return thread_id(thread_cur());
}

static int
sc_uname(struct utsname *u)
{
	if (!u_access_ok(u, sizeof(*u), PROT_WRITE))
		return DERR(-EFAULT);
	strcpy(u->sysname, "APEX RTOS");
	strcpy(u->nodename, "apex");
	strcpy(u->release, VERSION_STRING);
	strcpy(u->version, CONFIG_UNAME_VERSION);
	strcpy(u->machine, CONFIG_MACHINE_NAME);
	strcpy(u->domainname, "");
	return 0;
}

static int
sc_reboot(int magic, int magic2, int cmd, void *arg)
{
	if (magic != 0xfee1dead && magic2 != 672274793)
		return DERR(-EINVAL);

	switch (cmd) {
	case RB_AUTOBOOT:
		info("Restarting system.\n");
		machine_reset();
		break;
	case RB_HALT_SYSTEM:
	case RB_ENABLE_CAD:
	case RB_DISABLE_CAD:
	case RB_KEXEC:
		return DERR(-ENOSYS);
	case RB_POWER_OFF:
		info("Power down.\n");
		machine_poweroff();
		break;
	case RB_SW_SUSPEND:
		machine_suspend();
		break;
	default:
		return DERR(-EINVAL);
	}

	/* Linux kills caller? */
	proc_exit(task_cur(), 0);
	return 0;
}

static int
sc_nanosleep(const struct timespec *req, struct timespec *rem)
{
	if (!u_address(req) || (rem && !u_access_ok(rem, sizeof *rem, PROT_WRITE)))
		return DERR(-EFAULT);
	const uint64_t ns = ts_to_ns(req);
	if (!ns)
		return 0;
	const uint64_t r = timer_delay(ns);
	if (!r)
		return 0;
	if (rem)
		ns_to_ts(r, rem);
	return -EINTR_NORESTART;
}

static int
sc_clock_gettime(clockid_t id, struct timespec *ts)
{
	if (!u_access_ok(ts, sizeof *ts, PROT_WRITE))
		return DERR(-EFAULT);

	switch (id) {
	case CLOCK_REALTIME:
	case CLOCK_REALTIME_COARSE:
		/* TODO(time): real time */
	case CLOCK_MONOTONIC:
	case CLOCK_MONOTONIC_COARSE:
		/* TODO(time): monotonic with adjustments */
	case CLOCK_MONOTONIC_RAW:
		/* TODO(time): monotonic without adjustments */
		ns_to_ts(timer_monotonic(), ts);
		return 0;
	case CLOCK_BOOTTIME:
	case CLOCK_BOOTTIME_ALARM:
	case CLOCK_REALTIME_ALARM:
	case CLOCK_PROCESS_CPUTIME_ID:
	case CLOCK_THREAD_CPUTIME_ID:
	case CLOCK_SGI_CYCLE:
	case CLOCK_TAI:
	default:
		return DERR(-EINVAL);
	}
}

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
 * Pointers to read only memory can be verified using either:
 * - u_address -- checks that pointer points to userspace memory
 * - u_access_ok -- checks that the memory region is in userspace and accessible
 *
 * Pointers to writable memory must be verified using u_access_ok.
 *
 * However, note that even if access tests succeed a subsequent access could fail
 * if we context switch after the test and another thread unmaps the region.
 *
 * We make use of the MMU/MPU to trap and stub bad accesses to userspace.
 */
__fast_rodata __attribute__((used)) const void *const
syscall_table[SYSCALL_TABLE_SIZE] = {
	[SYS_access] = sc_access,
	[SYS_brk] = sc_brk,
	[SYS_chdir] = sc_chdir,
	[SYS_chmod] = sc_chmod,				/* stub */
	[SYS_chown32] = sc_chown,
	[SYS_clock_gettime] = sc_clock_gettime,		/* stub */
	[SYS_clone] = sc_clone,
	[SYS_close] = close,
	[SYS_dup2] = dup2,
	[SYS_dup] = dup,
	[SYS_execve] = sc_execve,
	[SYS_exit] = sc_exit,
	[SYS_exit_group] = sc_exit_group,
	[SYS_faccessat] = sc_faccessat,
	[SYS_fchmod] = fchmod,				/* stub */
	[SYS_fchmodat] = sc_fchmodat,			/* stub */
	[SYS_fchown32] = fchown,			/* stub */
	[SYS_fchownat] = sc_fchownat,			/* stub */
	[SYS_fcntl64] = sc_fcntl,
	[SYS_fork] = sc_fork,
	[SYS_fstat64] = sc_fstat,
	[SYS_fstatat64] = sc_fstatat,
	[SYS_fstatfs64] = sc_fstatfs,
	[SYS_fsync] = fsync,
	[SYS_futex] = sc_futex,
	[SYS_getcwd] = sc_getcwd,
	[SYS_getdents64] = sc_getdents,
	[SYS_geteuid32] = geteuid,
	[SYS_getitimer] = sc_getitimer,
	[SYS_getpgid] = getpgid,
	[SYS_getpid] = getpid,
	[SYS_getppid] = getppid,
	[SYS_getsid] = getsid,
	[SYS_getuid32] = getuid,			/* no user support */
	[SYS_ioctl] = sc_ioctl,
	[SYS_kill] = kill,
	[SYS_lchown32] = sc_lchown,
	[SYS_mkdir] = sc_mkdir,
	[SYS_mkdirat] = sc_mkdirat,
	[SYS_mknod] = sc_mknod,
	[SYS_mknodat] = sc_mknodat,
	[SYS_mmap2] = sc_mmap2,
	[SYS_mount] = sc_mount,
	[SYS_mprotect] = sc_mprotect,
	[SYS_munmap] = sc_munmap,
	[SYS_nanosleep] = sc_nanosleep,
	[SYS_open] = sc_open,
	[SYS_openat] = sc_openat,
	[SYS_pipe2] = sc_pipe2,
	[SYS_pipe] = sc_pipe,
	[SYS_pread64] = sc_pread,
	[SYS_preadv] = sc_preadv,
	[SYS_pwrite64] = sc_pwrite,
	[SYS_pwritev] = sc_pwritev,
	[SYS_read] = sc_read,
	[SYS_readlink] = sc_readlink,
	[SYS_readlinkat] = sc_readlinkat,
	[SYS_readv] = sc_readv,
	[SYS_reboot] = sc_reboot,
	[SYS_rename] = sc_rename,
	[SYS_renameat] = sc_renameat,
	[SYS_rmdir] = sc_rmdir,
	[SYS_rt_sigaction] = sc_rt_sigaction,
	[SYS_rt_sigprocmask] = sc_rt_sigprocmask,
	[SYS_rt_sigreturn] = sc_rt_sigreturn,
	[SYS_sched_get_priority_max] = sched_get_priority_max,
	[SYS_sched_get_priority_min] = sched_get_priority_min,
	[SYS_sched_yield] = sch_yield,
	[SYS_set_tid_address] = sc_set_tid_address,
	[SYS_setitimer] = sc_setitimer,
	[SYS_setpgid] = setpgid,
	[SYS_setsid] = setsid,
	[SYS_sigreturn] = sc_sigreturn,
	[SYS_stat64] = sc_stat,
	[SYS_statfs64] = sc_statfs,
	[SYS_symlink] = sc_symlink,
	[SYS_symlinkat] = sc_symlinkat,
	[SYS_sync] = sync,
	[SYS_syslog] = sc_syslog,
	[SYS_tgkill] = sc_tgkill,
	[SYS_tkill] = sc_tkill,
	[SYS_umask] = umask,
	[SYS_umount2] = sc_umount2,
	[SYS_uname] = sc_uname,
	[SYS_unlink] = sc_unlink,
	[SYS_unlinkat] = sc_unlinkat,
	[SYS_utimensat] = sc_utimensat,		    /* no time support in FS */
	[SYS_vfork] = sc_vfork,
	[SYS_wait4] = sc_wait4,
	[SYS_write] = sc_write,
	[SYS_writev] = sc_writev,
#if UINTPTR_MAX == 0xffffffff
	[SYS__llseek] = sc_llseek,
#else
	[SYS_lseek] = lseek,
#endif
};
