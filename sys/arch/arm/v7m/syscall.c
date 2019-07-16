#include "syscall.h"

#include <access.h>
#include <arch.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <syscall.h>
#include <task.h>
#include <thread.h>

const char *syscall_string(long);

/*
 * architecture specific syscalls for arm v7m.
 */
int
arch_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6,
    long sc)
{
	switch (sc) {
	case __ARM_NR_set_tls:
		if (!u_address((void*)a0))
			return DERR(-EFAULT);
		context_set_tls(&thread_cur()->ctx, (void*)a0);
		return 0;
	}
	dbg("WARNING: unimplemented syscall %ld\n", sc);
	return DERR(-ENOSYS);
}

/*
 * syscall_trace
 */
void
syscall_trace(long r0, long r1, long r2, long r3, long r4, long r5, long r6,
    long sc)
{
	dbg("SC: tsk %p th %p r0 %08lx r1 %08lx r2 %08lx r3 %08lx r4 "
	    "%08lx r5 %08lx r6 %08lx n %ld %s\n",
	    task_cur(), thread_cur(), r0, r1, r2, r3, r4, r5, r6, sc,
	    syscall_string(sc));
}

/*
 * syscall_trace_return
 */
void
syscall_trace_return(long rval, long sc)
{
	switch (rval) {
	case -EINTERRUPT_RETURN:
		return;
	case -ERESTARTSYS:
		dbg("RESTART: n %ld %s\n", sc, syscall_string(sc));
		break;
	default:
		dbg("SR: tsk %p th %p rval %08lx n %ld %s\n",
		    task_cur(), thread_cur(), rval, sc, syscall_string(sc));
		break;
	}
}

/*
 * string mappings for syscall numbers
 */
const char *
syscall_string(long sc)
{
	switch (sc) {
	case SYS_restart_syscall: return "restart_syscall";
	case SYS_exit: return "exit";
	case SYS_fork: return "fork";
	case SYS_read: return "read";
	case SYS_write: return "write";
	case SYS_open: return "open";
	case SYS_close: return "close";
	case SYS_creat: return "creat";
	case SYS_link: return "link";
	case SYS_unlink: return "unlink";
	case SYS_execve: return "execve";
	case SYS_chdir: return "chdir";
	case SYS_mknod: return "mknod";
	case SYS_chmod: return "chmod";
	case SYS_lchown: return "lchown";
	case SYS_lseek: return "lseek";
	case SYS_getpid: return "getpid";
	case SYS_mount: return "mount";
	case SYS_setuid: return "setuid";
	case SYS_getuid: return "getuid";
	case SYS_ptrace: return "ptrace";
	case SYS_pause: return "pause";
	case SYS_access: return "access";
	case SYS_nice: return "nice";
	case SYS_sync: return "sync";
	case SYS_kill: return "kill";
	case SYS_rename: return "rename";
	case SYS_mkdir: return "mkdir";
	case SYS_rmdir: return "rmdir";
	case SYS_dup: return "dup";
	case SYS_pipe: return "pipe";
	case SYS_times: return "times";
	case SYS_brk: return "brk";
	case SYS_setgid: return "setgid";
	case SYS_getgid: return "getgid";
	case SYS_geteuid: return "geteuid";
	case SYS_getegid: return "getegid";
	case SYS_acct: return "acct";
	case SYS_umount2: return "umount2";
	case SYS_ioctl: return "ioctl";
	case SYS_fcntl: return "fcntl";
	case SYS_setpgid: return "setpgid";
	case SYS_umask: return "umask";
	case SYS_chroot: return "chroot";
	case SYS_ustat: return "ustat";
	case SYS_dup2: return "dup2";
	case SYS_getppid: return "getppid";
	case SYS_getpgrp: return "getpgrp";
	case SYS_setsid: return "setsid";
	case SYS_sigaction: return "sigaction";
	case SYS_setreuid: return "setreuid";
	case SYS_setregid: return "setregid";
	case SYS_sigsuspend: return "sigsuspend";
	case SYS_sigpending: return "sigpending";
	case SYS_sethostname: return "sethostname";
	case SYS_setrlimit: return "setrlimit";
	case SYS_getrusage: return "getrusage";
	case SYS_gettimeofday: return "gettimeofday";
	case SYS_settimeofday: return "settimeofday";
	case SYS_getgroups: return "getgroups";
	case SYS_setgroups: return "setgroups";
	case SYS_symlink: return "symlink";
	case SYS_readlink: return "readlink";
	case SYS_uselib: return "uselib";
	case SYS_swapon: return "swapon";
	case SYS_reboot: return "reboot";
	case SYS_munmap: return "munmap";
	case SYS_truncate: return "truncate";
	case SYS_ftruncate: return "ftruncate";
	case SYS_fchmod: return "fchmod";
	case SYS_fchown: return "fchown";
	case SYS_getpriority: return "getpriority";
	case SYS_setpriority: return "setpriority";
	case SYS_statfs: return "statfs";
	case SYS_fstatfs: return "fstatfs";
	case SYS_syslog: return "syslog";
	case SYS_setitimer: return "setitimer";
	case SYS_getitimer: return "getitimer";
	case SYS_stat: return "stat";
	case SYS_lstat: return "lstat";
	case SYS_fstat: return "fstat";
	case SYS_vhangup: return "vhangup";
	case SYS_wait4: return "wait4";
	case SYS_swapoff: return "swapoff";
	case SYS_sysinfo: return "sysinfo";
	case SYS_fsync: return "fsync";
	case SYS_sigreturn: return "sigreturn";
	case SYS_clone: return "clone";
	case SYS_setdomainname: return "setdomainname";
	case SYS_uname: return "uname";
	case SYS_adjtimex: return "adjtimex";
	case SYS_mprotect: return "mprotect";
	case SYS_sigprocmask: return "sigprocmask";
	case SYS_init_module: return "init_module";
	case SYS_delete_module: return "delete_module";
	case SYS_quotactl: return "quotactl";
	case SYS_getpgid: return "getpgid";
	case SYS_fchdir: return "fchdir";
	case SYS_bdflush: return "bdflush";
	case SYS_sysfs: return "sysfs";
	case SYS_personality: return "personality";
	case SYS_setfsuid: return "setfsuid";
	case SYS_setfsgid: return "setfsgid";
	case SYS__llseek: return "_llseek";
	case SYS_getdents: return "getdents";
	case SYS__newselect: return "_newselect";
	case SYS_flock: return "flock";
	case SYS_msync: return "msync";
	case SYS_readv: return "readv";
	case SYS_writev: return "writev";
	case SYS_getsid: return "getsid";
	case SYS_fdatasync: return "fdatasync";
	case SYS__sysctl: return "_sysctl";
	case SYS_mlock: return "mlock";
	case SYS_munlock: return "munlock";
	case SYS_mlockall: return "mlockall";
	case SYS_munlockall: return "munlockall";
	case SYS_sched_setparam: return "sched_setparam";
	case SYS_sched_getparam: return "sched_getparam";
	case SYS_sched_setscheduler: return "sched_setscheduler";
	case SYS_sched_getscheduler: return "sched_getscheduler";
	case SYS_sched_yield: return "sched_yield";
	case SYS_sched_get_priority_max: return "sched_get_priority_max";
	case SYS_sched_get_priority_min: return "sched_get_priority_min";
	case SYS_sched_rr_get_interval: return "sched_rr_get_interval";
	case SYS_nanosleep: return "nanosleep";
	case SYS_mremap: return "mremap";
	case SYS_setresuid: return "setresuid";
	case SYS_getresuid: return "getresuid";
	case SYS_poll: return "poll";
	case SYS_nfsservctl: return "nfsservctl";
	case SYS_setresgid: return "setresgid";
	case SYS_getresgid: return "getresgid";
	case SYS_prctl: return "prctl";
	case SYS_rt_sigreturn: return "rt_sigreturn";
	case SYS_rt_sigaction: return "rt_sigaction";
	case SYS_rt_sigprocmask: return "rt_sigprocmask";
	case SYS_rt_sigpending: return "rt_sigpending";
	case SYS_rt_sigtimedwait: return "rt_sigtimedwait";
	case SYS_rt_sigqueueinfo: return "rt_sigqueueinfo";
	case SYS_rt_sigsuspend: return "rt_sigsuspend";
	case SYS_pread64: return "pread64";
	case SYS_pwrite64: return "pwrite64";
	case SYS_chown: return "chown";
	case SYS_getcwd: return "getcwd";
	case SYS_capget: return "capget";
	case SYS_capset: return "capset";
	case SYS_sigaltstack: return "sigaltstack";
	case SYS_sendfile: return "sendfile";
	case SYS_vfork: return "vfork";
	case SYS_ugetrlimit: return "ugetrlimit";
	case SYS_mmap2: return "mmap2";
	case SYS_truncate64: return "truncate64";
	case SYS_ftruncate64: return "ftruncate64";
	case SYS_stat64: return "stat64";
	case SYS_lstat64: return "lstat64";
	case SYS_fstat64: return "fstat64";
	case SYS_lchown32: return "lchown32";
	case SYS_getuid32: return "getuid32";
	case SYS_getgid32: return "getgid32";
	case SYS_geteuid32: return "geteuid32";
	case SYS_getegid32: return "getegid32";
	case SYS_setreuid32: return "setreuid32";
	case SYS_setregid32: return "setregid32";
	case SYS_getgroups32: return "getgroups32";
	case SYS_setgroups32: return "setgroups32";
	case SYS_fchown32: return "fchown32";
	case SYS_setresuid32: return "setresuid32";
	case SYS_getresuid32: return "getresuid32";
	case SYS_setresgid32: return "setresgid32";
	case SYS_getresgid32: return "getresgid32";
	case SYS_chown32: return "chown32";
	case SYS_setuid32: return "setuid32";
	case SYS_setgid32: return "setgid32";
	case SYS_setfsuid32: return "setfsuid32";
	case SYS_setfsgid32: return "setfsgid32";
	case SYS_getdents64: return "getdents64";
	case SYS_pivot_root: return "pivot_root";
	case SYS_mincore: return "mincore";
	case SYS_madvise: return "madvise";
	case SYS_fcntl64: return "fcntl64";
	case SYS_gettid: return "gettid";
	case SYS_readahead: return "readahead";
	case SYS_setxattr: return "setxattr";
	case SYS_lsetxattr: return "lsetxattr";
	case SYS_fsetxattr: return "fsetxattr";
	case SYS_getxattr: return "getxattr";
	case SYS_lgetxattr: return "lgetxattr";
	case SYS_fgetxattr: return "fgetxattr";
	case SYS_listxattr: return "listxattr";
	case SYS_llistxattr: return "llistxattr";
	case SYS_flistxattr: return "flistxattr";
	case SYS_removexattr: return "removexattr";
	case SYS_lremovexattr: return "lremovexattr";
	case SYS_fremovexattr: return "fremovexattr";
	case SYS_tkill: return "tkill";
	case SYS_sendfile64: return "sendfile64";
	case SYS_futex: return "futex";
	case SYS_sched_setaffinity: return "sched_setaffinity";
	case SYS_sched_getaffinity: return "sched_getaffinity";
	case SYS_io_setup: return "io_setup";
	case SYS_io_destroy: return "io_destroy";
	case SYS_io_getevents: return "io_getevents";
	case SYS_io_submit: return "io_submit";
	case SYS_io_cancel: return "io_cancel";
	case SYS_exit_group: return "exit_group";
	case SYS_lookup_dcookie: return "lookup_dcookie";
	case SYS_epoll_create: return "epoll_create";
	case SYS_epoll_ctl: return "epoll_ctl";
	case SYS_epoll_wait: return "epoll_wait";
	case SYS_remap_file_pages: return "remap_file_pages";
	case SYS_set_tid_address: return "set_tid_address";
	case SYS_timer_create: return "timer_create";
	case SYS_timer_settime: return "timer_settime";
	case SYS_timer_gettime: return "timer_gettime";
	case SYS_timer_getoverrun: return "timer_getoverrun";
	case SYS_timer_delete: return "timer_delete";
	case SYS_clock_settime: return "clock_settime";
	case SYS_clock_gettime: return "clock_gettime";
	case SYS_clock_getres: return "clock_getres";
	case SYS_clock_nanosleep: return "clock_nanosleep";
	case SYS_statfs64: return "statfs64";
	case SYS_fstatfs64: return "fstatfs64";
	case SYS_tgkill: return "tgkill";
	case SYS_utimes: return "utimes";
	case SYS_fadvise64_64: return "fadvise64_64";
	case SYS_pciconfig_iobase: return "pciconfig_iobase";
	case SYS_pciconfig_read: return "pciconfig_read";
	case SYS_pciconfig_write: return "pciconfig_write";
	case SYS_mq_open: return "mq_open";
	case SYS_mq_unlink: return "mq_unlink";
	case SYS_mq_timedsend: return "mq_timedsend";
	case SYS_mq_timedreceive: return "mq_timedreceive";
	case SYS_mq_notify: return "mq_notify";
	case SYS_mq_getsetattr: return "mq_getsetattr";
	case SYS_waitid: return "waitid";
	case SYS_socket: return "socket";
	case SYS_bind: return "bind";
	case SYS_connect: return "connect";
	case SYS_listen: return "listen";
	case SYS_accept: return "accept";
	case SYS_getsockname: return "getsockname";
	case SYS_getpeername: return "getpeername";
	case SYS_socketpair: return "socketpair";
	case SYS_send: return "send";
	case SYS_sendto: return "sendto";
	case SYS_recv: return "recv";
	case SYS_recvfrom: return "recvfrom";
	case SYS_shutdown: return "shutdown";
	case SYS_setsockopt: return "setsockopt";
	case SYS_getsockopt: return "getsockopt";
	case SYS_sendmsg: return "sendmsg";
	case SYS_recvmsg: return "recvmsg";
	case SYS_semop: return "semop";
	case SYS_semget: return "semget";
	case SYS_semctl: return "semctl";
	case SYS_msgsnd: return "msgsnd";
	case SYS_msgrcv: return "msgrcv";
	case SYS_msgget: return "msgget";
	case SYS_msgctl: return "msgctl";
	case SYS_shmat: return "shmat";
	case SYS_shmdt: return "shmdt";
	case SYS_shmget: return "shmget";
	case SYS_shmctl: return "shmctl";
	case SYS_add_key: return "add_key";
	case SYS_request_key: return "request_key";
	case SYS_keyctl: return "keyctl";
	case SYS_semtimedop: return "semtimedop";
	case SYS_vserver: return "vserver";
	case SYS_ioprio_set: return "ioprio_set";
	case SYS_ioprio_get: return "ioprio_get";
	case SYS_inotify_init: return "inotify_init";
	case SYS_inotify_add_watch: return "inotify_add_watch";
	case SYS_inotify_rm_watch: return "inotify_rm_watch";
	case SYS_mbind: return "mbind";
	case SYS_get_mempolicy: return "get_mempolicy";
	case SYS_set_mempolicy: return "set_mempolicy";
	case SYS_openat: return "openat";
	case SYS_mkdirat: return "mkdirat";
	case SYS_mknodat: return "mknodat";
	case SYS_fchownat: return "fchownat";
	case SYS_futimesat: return "futimesat";
	case SYS_fstatat64: return "fstatat64";
	case SYS_unlinkat: return "unlinkat";
	case SYS_renameat: return "renameat";
	case SYS_linkat: return "linkat";
	case SYS_symlinkat: return "symlinkat";
	case SYS_readlinkat: return "readlinkat";
	case SYS_fchmodat: return "fchmodat";
	case SYS_faccessat: return "faccessat";
	case SYS_pselect6: return "pselect6";
	case SYS_ppoll: return "ppoll";
	case SYS_unshare: return "unshare";
	case SYS_set_robust_list: return "set_robust_list";
	case SYS_get_robust_list: return "get_robust_list";
	case SYS_splice: return "splice";
	case SYS_sync_file_range2: return "sync_file_range2";
	case SYS_tee: return "tee";
	case SYS_vmsplice: return "vmsplice";
	case SYS_move_pages: return "move_pages";
	case SYS_getcpu: return "getcpu";
	case SYS_epoll_pwait: return "epoll_pwait";
	case SYS_kexec_load: return "kexec_load";
	case SYS_utimensat: return "utimensat";
	case SYS_signalfd: return "signalfd";
	case SYS_timerfd_create: return "timerfd_create";
	case SYS_eventfd: return "eventfd";
	case SYS_fallocate: return "fallocate";
	case SYS_timerfd_settime: return "timerfd_settime";
	case SYS_timerfd_gettime: return "timerfd_gettime";
	case SYS_signalfd4: return "signalfd4";
	case SYS_eventfd2: return "eventfd2";
	case SYS_epoll_create1: return "epoll_create1";
	case SYS_dup3: return "dup3";
	case SYS_pipe2: return "pipe2";
	case SYS_inotify_init1: return "inotify_init1";
	case SYS_preadv: return "preadv";
	case SYS_pwritev: return "pwritev";
	case SYS_rt_tgsigqueueinfo: return "rt_tgsigqueueinfo";
	case SYS_perf_event_open: return "perf_event_open";
	case SYS_recvmmsg: return "recvmmsg";
	case SYS_accept4: return "accept4";
	case SYS_fanotify_init: return "fanotify_init";
	case SYS_fanotify_mark: return "fanotify_mark";
	case SYS_prlimit64: return "prlimit64";
	case SYS_name_to_handle_at: return "name_to_handle_at";
	case SYS_open_by_handle_at: return "open_by_handle_at";
	case SYS_clock_adjtime: return "clock_adjtime";
	case SYS_syncfs: return "syncfs";
	case SYS_sendmmsg: return "sendmmsg";
	case SYS_setns: return "setns";
	case SYS_process_vm_readv: return "process_vm_readv";
	case SYS_process_vm_writev: return "process_vm_writev";
	case SYS_kcmp: return "kcmp";
	case SYS_finit_module: return "finit_module";
	case SYS_sched_setattr: return "sched_setattr";
	case SYS_sched_getattr: return "sched_getattr";
	case SYS_renameat2: return "renameat2";
	case SYS_seccomp: return "seccomp";
	case SYS_getrandom: return "getrandom";
	case SYS_memfd_create: return "memfd_create";
	case SYS_bpf: return "bpf";
	case SYS_execveat: return "execveat";
	case SYS_userfaultfd: return "userfaultfd";
	case SYS_membarrier: return "membarrier";
	case SYS_mlock2: return "mlock2";
	case SYS_copy_file_range: return "copy_file_range";
	case SYS_preadv2: return "preadv2";
	case SYS_pwritev2: return "pwritev2";
	case SYS_pkey_mprotect: return "pkey_mprotect";
	case SYS_pkey_alloc: return "pkey_alloc";
	case SYS_pkey_free: return "pkey_free";
	case SYS_statx: return "statx";
	case __ARM_NR_breakpoint: return "ARM_breakpoint";
	case __ARM_NR_cacheflush: return "ARM_cacheflush";
	case __ARM_NR_usr26: return "ARM_usr26";
	case __ARM_NR_usr32: return "ARM_usr32";
	case __ARM_NR_set_tls: return "ARM_set_tls";
	case __ARM_NR_get_tls: return "ARM_get_tls";
	default: return "UNKNOWN";
	}
}
