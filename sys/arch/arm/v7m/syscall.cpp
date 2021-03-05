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
	case __NR_restart_syscall: return "restart_syscall";
	case __NR_exit: return "exit";
	case __NR_fork: return "fork";
	case __NR_read: return "read";
	case __NR_write: return "write";
	case __NR_open: return "open";
	case __NR_close: return "close";
	case __NR_creat: return "creat";
	case __NR_link: return "link";
	case __NR_unlink: return "unlink";
	case __NR_execve: return "execve";
	case __NR_chdir: return "chdir";
	case __NR_mknod: return "mknod";
	case __NR_chmod: return "chmod";
	case __NR_lchown: return "lchown";
	case __NR_lseek: return "lseek";
	case __NR_getpid: return "getpid";
	case __NR_mount: return "mount";
	case __NR_setuid: return "setuid";
	case __NR_getuid: return "getuid";
	case __NR_ptrace: return "ptrace";
	case __NR_pause: return "pause";
	case __NR_access: return "access";
	case __NR_nice: return "nice";
	case __NR_sync: return "sync";
	case __NR_kill: return "kill";
	case __NR_rename: return "rename";
	case __NR_mkdir: return "mkdir";
	case __NR_rmdir: return "rmdir";
	case __NR_dup: return "dup";
	case __NR_pipe: return "pipe";
	case __NR_times: return "times";
	case __NR_brk: return "brk";
	case __NR_setgid: return "setgid";
	case __NR_getgid: return "getgid";
	case __NR_geteuid: return "geteuid";
	case __NR_getegid: return "getegid";
	case __NR_acct: return "acct";
	case __NR_umount2: return "umount2";
	case __NR_ioctl: return "ioctl";
	case __NR_fcntl: return "fcntl";
	case __NR_setpgid: return "setpgid";
	case __NR_umask: return "umask";
	case __NR_chroot: return "chroot";
	case __NR_ustat: return "ustat";
	case __NR_dup2: return "dup2";
	case __NR_getppid: return "getppid";
	case __NR_getpgrp: return "getpgrp";
	case __NR_setsid: return "setsid";
	case __NR_sigaction: return "sigaction";
	case __NR_setreuid: return "setreuid";
	case __NR_setregid: return "setregid";
	case __NR_sigsuspend: return "sigsuspend";
	case __NR_sigpending: return "sigpending";
	case __NR_sethostname: return "sethostname";
	case __NR_setrlimit: return "setrlimit";
	case __NR_getrusage: return "getrusage";
	case __NR_gettimeofday_time32: return "gettimeofday_time32";
	case __NR_settimeofday_time32: return "settimeofday_time32";
	case __NR_getgroups: return "getgroups";
	case __NR_setgroups: return "setgroups";
	case __NR_symlink: return "symlink";
	case __NR_readlink: return "readlink";
	case __NR_uselib: return "uselib";
	case __NR_swapon: return "swapon";
	case __NR_reboot: return "reboot";
	case __NR_munmap: return "munmap";
	case __NR_truncate: return "truncate";
	case __NR_ftruncate: return "ftruncate";
	case __NR_fchmod: return "fchmod";
	case __NR_fchown: return "fchown";
	case __NR_getpriority: return "getpriority";
	case __NR_setpriority: return "setpriority";
	case __NR_statfs: return "statfs";
	case __NR_fstatfs: return "fstatfs";
	case __NR_syslog: return "syslog";
	case __NR_setitimer: return "setitimer";
	case __NR_getitimer: return "getitimer";
	case __NR_stat: return "stat";
	case __NR_lstat: return "lstat";
	case __NR_fstat: return "fstat";
	case __NR_vhangup: return "vhangup";
	case __NR_wait4: return "wait4";
	case __NR_swapoff: return "swapoff";
	case __NR_sysinfo: return "sysinfo";
	case __NR_fsync: return "fsync";
	case __NR_sigreturn: return "sigreturn";
	case __NR_clone: return "clone";
	case __NR_setdomainname: return "setdomainname";
	case __NR_uname: return "uname";
	case __NR_adjtimex: return "adjtimex";
	case __NR_mprotect: return "mprotect";
	case __NR_sigprocmask: return "sigprocmask";
	case __NR_init_module: return "init_module";
	case __NR_delete_module: return "delete_module";
	case __NR_quotactl: return "quotactl";
	case __NR_getpgid: return "getpgid";
	case __NR_fchdir: return "fchdir";
	case __NR_bdflush: return "bdflush";
	case __NR_sysfs: return "sysfs";
	case __NR_personality: return "personality";
	case __NR_setfsuid: return "setfsuid";
	case __NR_setfsgid: return "setfsgid";
	case __NR__llseek: return "_llseek";
	case __NR_getdents: return "getdents";
	case __NR__newselect: return "_newselect";
	case __NR_flock: return "flock";
	case __NR_msync: return "msync";
	case __NR_readv: return "readv";
	case __NR_writev: return "writev";
	case __NR_getsid: return "getsid";
	case __NR_fdatasync: return "fdatasync";
	case __NR__sysctl: return "_sysctl";
	case __NR_mlock: return "mlock";
	case __NR_munlock: return "munlock";
	case __NR_mlockall: return "mlockall";
	case __NR_munlockall: return "munlockall";
	case __NR_sched_setparam: return "sched_setparam";
	case __NR_sched_getparam: return "sched_getparam";
	case __NR_sched_setscheduler: return "sched_setscheduler";
	case __NR_sched_getscheduler: return "sched_getscheduler";
	case __NR_sched_yield: return "sched_yield";
	case __NR_sched_get_priority_max: return "sched_get_priority_max";
	case __NR_sched_get_priority_min: return "sched_get_priority_min";
	case __NR_sched_rr_get_interval: return "sched_rr_get_interval";
	case __NR_nanosleep: return "nanosleep";
	case __NR_mremap: return "mremap";
	case __NR_setresuid: return "setresuid";
	case __NR_getresuid: return "getresuid";
	case __NR_poll: return "poll";
	case __NR_nfsservctl: return "nfsservctl";
	case __NR_setresgid: return "setresgid";
	case __NR_getresgid: return "getresgid";
	case __NR_prctl: return "prctl";
	case __NR_rt_sigreturn: return "rt_sigreturn";
	case __NR_rt_sigaction: return "rt_sigaction";
	case __NR_rt_sigprocmask: return "rt_sigprocmask";
	case __NR_rt_sigpending: return "rt_sigpending";
	case __NR_rt_sigtimedwait: return "rt_sigtimedwait";
	case __NR_rt_sigqueueinfo: return "rt_sigqueueinfo";
	case __NR_rt_sigsuspend: return "rt_sigsuspend";
	case __NR_pread64: return "pread64";
	case __NR_pwrite64: return "pwrite64";
	case __NR_chown: return "chown";
	case __NR_getcwd: return "getcwd";
	case __NR_capget: return "capget";
	case __NR_capset: return "capset";
	case __NR_sigaltstack: return "sigaltstack";
	case __NR_sendfile: return "sendfile";
	case __NR_vfork: return "vfork";
	case __NR_ugetrlimit: return "ugetrlimit";
	case __NR_mmap2: return "mmap2";
	case __NR_truncate64: return "truncate64";
	case __NR_ftruncate64: return "ftruncate64";
	case __NR_stat64: return "stat64";
	case __NR_lstat64: return "lstat64";
	case __NR_fstat64: return "fstat64";
	case __NR_lchown32: return "lchown32";
	case __NR_getuid32: return "getuid32";
	case __NR_getgid32: return "getgid32";
	case __NR_geteuid32: return "geteuid32";
	case __NR_getegid32: return "getegid32";
	case __NR_setreuid32: return "setreuid32";
	case __NR_setregid32: return "setregid32";
	case __NR_getgroups32: return "getgroups32";
	case __NR_setgroups32: return "setgroups32";
	case __NR_fchown32: return "fchown32";
	case __NR_setresuid32: return "setresuid32";
	case __NR_getresuid32: return "getresuid32";
	case __NR_setresgid32: return "setresgid32";
	case __NR_getresgid32: return "getresgid32";
	case __NR_chown32: return "chown32";
	case __NR_setuid32: return "setuid32";
	case __NR_setgid32: return "setgid32";
	case __NR_setfsuid32: return "setfsuid32";
	case __NR_setfsgid32: return "setfsgid32";
	case __NR_getdents64: return "getdents64";
	case __NR_pivot_root: return "pivot_root";
	case __NR_mincore: return "mincore";
	case __NR_madvise: return "madvise";
	case __NR_fcntl64: return "fcntl64";
	case __NR_gettid: return "gettid";
	case __NR_readahead: return "readahead";
	case __NR_setxattr: return "setxattr";
	case __NR_lsetxattr: return "lsetxattr";
	case __NR_fsetxattr: return "fsetxattr";
	case __NR_getxattr: return "getxattr";
	case __NR_lgetxattr: return "lgetxattr";
	case __NR_fgetxattr: return "fgetxattr";
	case __NR_listxattr: return "listxattr";
	case __NR_llistxattr: return "llistxattr";
	case __NR_flistxattr: return "flistxattr";
	case __NR_removexattr: return "removexattr";
	case __NR_lremovexattr: return "lremovexattr";
	case __NR_fremovexattr: return "fremovexattr";
	case __NR_tkill: return "tkill";
	case __NR_sendfile64: return "sendfile64";
	case __NR_futex: return "futex";
	case __NR_sched_setaffinity: return "sched_setaffinity";
	case __NR_sched_getaffinity: return "sched_getaffinity";
	case __NR_io_setup: return "io_setup";
	case __NR_io_destroy: return "io_destroy";
	case __NR_io_getevents: return "io_getevents";
	case __NR_io_submit: return "io_submit";
	case __NR_io_cancel: return "io_cancel";
	case __NR_exit_group: return "exit_group";
	case __NR_lookup_dcookie: return "lookup_dcookie";
	case __NR_epoll_create: return "epoll_create";
	case __NR_epoll_ctl: return "epoll_ctl";
	case __NR_epoll_wait: return "epoll_wait";
	case __NR_remap_file_pages: return "remap_file_pages";
	case __NR_set_tid_address: return "set_tid_address";
	case __NR_timer_create: return "timer_create";
	case __NR_timer_settime32: return "timer_settime32";
	case __NR_timer_gettime32: return "timer_gettime32";
	case __NR_timer_getoverrun: return "timer_getoverrun";
	case __NR_timer_delete: return "timer_delete";
	case __NR_clock_settime32: return "clock_settime32";
	case __NR_clock_gettime32: return "clock_gettime32";
	case __NR_clock_getres_time32: return "clock_getres_time32";
	case __NR_clock_nanosleep_time32: return "clock_nanosleep_time32";
	case __NR_statfs64: return "statfs64";
	case __NR_fstatfs64: return "fstatfs64";
	case __NR_tgkill: return "tgkill";
	case __NR_utimes: return "utimes";
	case __NR_fadvise64_64: return "fadvise64_64";
	case __NR_pciconfig_iobase: return "pciconfig_iobase";
	case __NR_pciconfig_read: return "pciconfig_read";
	case __NR_pciconfig_write: return "pciconfig_write";
	case __NR_mq_open: return "mq_open";
	case __NR_mq_unlink: return "mq_unlink";
	case __NR_mq_timedsend: return "mq_timedsend";
	case __NR_mq_timedreceive: return "mq_timedreceive";
	case __NR_mq_notify: return "mq_notify";
	case __NR_mq_getsetattr: return "mq_getsetattr";
	case __NR_waitid: return "waitid";
	case __NR_socket: return "socket";
	case __NR_bind: return "bind";
	case __NR_connect: return "connect";
	case __NR_listen: return "listen";
	case __NR_accept: return "accept";
	case __NR_getsockname: return "getsockname";
	case __NR_getpeername: return "getpeername";
	case __NR_socketpair: return "socketpair";
	case __NR_send: return "send";
	case __NR_sendto: return "sendto";
	case __NR_recv: return "recv";
	case __NR_recvfrom: return "recvfrom";
	case __NR_shutdown: return "shutdown";
	case __NR_setsockopt: return "setsockopt";
	case __NR_getsockopt: return "getsockopt";
	case __NR_sendmsg: return "sendmsg";
	case __NR_recvmsg: return "recvmsg";
	case __NR_semop: return "semop";
	case __NR_semget: return "semget";
	case __NR_semctl: return "semctl";
	case __NR_msgsnd: return "msgsnd";
	case __NR_msgrcv: return "msgrcv";
	case __NR_msgget: return "msgget";
	case __NR_msgctl: return "msgctl";
	case __NR_shmat: return "shmat";
	case __NR_shmdt: return "shmdt";
	case __NR_shmget: return "shmget";
	case __NR_shmctl: return "shmctl";
	case __NR_add_key: return "add_key";
	case __NR_request_key: return "request_key";
	case __NR_keyctl: return "keyctl";
	case __NR_semtimedop: return "semtimedop";
	case __NR_vserver: return "vserver";
	case __NR_ioprio_set: return "ioprio_set";
	case __NR_ioprio_get: return "ioprio_get";
	case __NR_inotify_init: return "inotify_init";
	case __NR_inotify_add_watch: return "inotify_add_watch";
	case __NR_inotify_rm_watch: return "inotify_rm_watch";
	case __NR_mbind: return "mbind";
	case __NR_get_mempolicy: return "get_mempolicy";
	case __NR_set_mempolicy: return "set_mempolicy";
	case __NR_openat: return "openat";
	case __NR_mkdirat: return "mkdirat";
	case __NR_mknodat: return "mknodat";
	case __NR_fchownat: return "fchownat";
	case __NR_futimesat: return "futimesat";
	case __NR_fstatat64: return "fstatat64";
	case __NR_unlinkat: return "unlinkat";
	case __NR_renameat: return "renameat";
	case __NR_linkat: return "linkat";
	case __NR_symlinkat: return "symlinkat";
	case __NR_readlinkat: return "readlinkat";
	case __NR_fchmodat: return "fchmodat";
	case __NR_faccessat: return "faccessat";
	case __NR_pselect6: return "pselect6";
	case __NR_ppoll: return "ppoll";
	case __NR_unshare: return "unshare";
	case __NR_set_robust_list: return "set_robust_list";
	case __NR_get_robust_list: return "get_robust_list";
	case __NR_splice: return "splice";
	case __NR_sync_file_range2: return "sync_file_range2";
	case __NR_tee: return "tee";
	case __NR_vmsplice: return "vmsplice";
	case __NR_move_pages: return "move_pages";
	case __NR_getcpu: return "getcpu";
	case __NR_epoll_pwait: return "epoll_pwait";
	case __NR_kexec_load: return "kexec_load";
	case __NR_utimensat: return "utimensat";
	case __NR_signalfd: return "signalfd";
	case __NR_timerfd_create: return "timerfd_create";
	case __NR_eventfd: return "eventfd";
	case __NR_fallocate: return "fallocate";
	case __NR_timerfd_settime32: return "timerfd_settime32";
	case __NR_timerfd_gettime32: return "timerfd_gettime32";
	case __NR_signalfd4: return "signalfd4";
	case __NR_eventfd2: return "eventfd2";
	case __NR_epoll_create1: return "epoll_create1";
	case __NR_dup3: return "dup3";
	case __NR_pipe2: return "pipe2";
	case __NR_inotify_init1: return "inotify_init1";
	case __NR_preadv: return "preadv";
	case __NR_pwritev: return "pwritev";
	case __NR_rt_tgsigqueueinfo: return "rt_tgsigqueueinfo";
	case __NR_perf_event_open: return "perf_event_open";
	case __NR_recvmmsg: return "recvmmsg";
	case __NR_accept4: return "accept4";
	case __NR_fanotify_init: return "fanotify_init";
	case __NR_fanotify_mark: return "fanotify_mark";
	case __NR_prlimit64: return "prlimit64";
	case __NR_name_to_handle_at: return "name_to_handle_at";
	case __NR_open_by_handle_at: return "open_by_handle_at";
	case __NR_clock_adjtime: return "clock_adjtime";
	case __NR_syncfs: return "syncfs";
	case __NR_sendmmsg: return "sendmmsg";
	case __NR_setns: return "setns";
	case __NR_process_vm_readv: return "process_vm_readv";
	case __NR_process_vm_writev: return "process_vm_writev";
	case __NR_kcmp: return "kcmp";
	case __NR_finit_module: return "finit_module";
	case __NR_sched_setattr: return "sched_setattr";
	case __NR_sched_getattr: return "sched_getattr";
	case __NR_renameat2: return "renameat2";
	case __NR_seccomp: return "seccomp";
	case __NR_getrandom: return "getrandom";
	case __NR_memfd_create: return "memfd_create";
	case __NR_bpf: return "bpf";
	case __NR_execveat: return "execveat";
	case __NR_userfaultfd: return "userfaultfd";
	case __NR_membarrier: return "membarrier";
	case __NR_mlock2: return "mlock2";
	case __NR_copy_file_range: return "copy_file_range";
	case __NR_preadv2: return "preadv2";
	case __NR_pwritev2: return "pwritev2";
	case __NR_pkey_mprotect: return "pkey_mprotect";
	case __NR_pkey_alloc: return "pkey_alloc";
	case __NR_pkey_free: return "pkey_free";
	case __NR_statx: return "statx";
	case __NR_rseq: return "rseq";
	case __NR_io_pgetevents: return "io_pgetevents";
	case __NR_migrate_pages: return "migrate_pages";
	case __NR_kexec_file_load: return "kexec_file_load";
	case __NR_clock_gettime64: return "clock_gettime64";
	case __NR_clock_settime64: return "clock_settime64";
	case __NR_clock_adjtime64: return "clock_adjtime64";
	case __NR_clock_getres_time64: return "clock_getres_time64";
	case __NR_clock_nanosleep_time64: return "clock_nanosleep_time64";
	case __NR_timer_gettime64: return "timer_gettime64";
	case __NR_timer_settime64: return "timer_settime64";
	case __NR_timerfd_gettime64: return "timerfd_gettime64";
	case __NR_timerfd_settime64: return "timerfd_settime64";
	case __NR_utimensat_time64: return "utimensat_time64";
	case __NR_pselect6_time64: return "pselect6_time64";
	case __NR_ppoll_time64: return "ppoll_time64";
	case __NR_io_pgetevents_time64: return "io_pgetevents_time64";
	case __NR_recvmmsg_time64: return "recvmmsg_time64";
	case __NR_mq_timedsend_time64: return "mq_timedsend_time64";
	case __NR_mq_timedreceive_time64: return "mq_timedreceive_time64";
	case __NR_semtimedop_time64: return "semtimedop_time64";
	case __NR_rt_sigtimedwait_time64: return "rt_sigtimedwait_time64";
	case __NR_futex_time64: return "futex_time64";
	case __NR_sched_rr_get_interval_time64: return "sched_rr_get_interval_time64";
	case __NR_pidfd_send_signal: return "pidfd_send_signal";
	case __NR_io_uring_setup: return "io_uring_setup";
	case __NR_io_uring_enter: return "io_uring_enter";
	case __NR_io_uring_register: return "io_uring_register";
	case __NR_open_tree: return "open_tree";
	case __NR_move_mount: return "move_mount";
	case __NR_fsopen: return "fsopen";
	case __NR_fsconfig: return "fsconfig";
	case __NR_fsmount: return "fsmount";
	case __NR_fspick: return "fspick";
	case __NR_pidfd_open: return "pidfd_open";
	case __NR_clone3: return "clone3";
	case __ARM_NR_breakpoint: return "NR_breakpoint";
	case __ARM_NR_cacheflush: return "NR_cacheflush";
	case __ARM_NR_usr26: return "NR_usr26";
	case __ARM_NR_usr32: return "NR_usr32";
	case __ARM_NR_set_tls: return "NR_set_tls";
	case __ARM_NR_get_tls: return "NR_get_tls";
	default: return "UNKNOWN";
	}
}
