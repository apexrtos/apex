#include <syscalls.h>

#include <access.h>
#include <arch.h>
#include <debug.h>
#include <errno.h>
#include <proc.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <task.h>
#include <thread.h>
#include <time.h>
#include <version.h>
#include <vm.h>

void
sc_exit()
{
	struct thread *th = thread_cur();

	if (th->clear_child_tid) {
		const int zero = 0;
		vm_write(th->task->as, &zero, th->clear_child_tid, sizeof zero);
		futex(th->task, th->clear_child_tid, FUTEX_PRIVATE | FUTEX_WAKE,
		    1, 0, 0);
	}

	thread_terminate(th);
}

void
sc_exit_group(int status)
{
	proc_exit(task_cur(), status);
}

int
sc_set_tid_address(int *p)
{
	/* no point taking u_access_lock here, we're not going to access the
	   pointer until the thread exits */
	if (!u_address(p))
		return DERR(-EFAULT);
	thread_cur()->clear_child_tid = p;
	return thread_id(thread_cur());
}

int
sc_uname(utsname *u)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
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

int
sc_reboot(unsigned long magic, unsigned long magic2, int cmd, void *arg)
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

int
sc_nanosleep(const timespec *req, timespec *rem)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_access_ok(req, sizeof *req, PROT_READ) ||
	    (rem && !u_access_ok(rem, sizeof *rem, PROT_WRITE)))
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

int
sc_clock_gettime(clockid_t id, timespec *ts)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_access_ok(ts, sizeof *ts, PROT_WRITE))
		return DERR(-EFAULT);
	switch (id) {
	case CLOCK_REALTIME:
	case CLOCK_REALTIME_COARSE:
		/* TODO(time): real time */
	case CLOCK_MONOTONIC:
		/* TODO(time): monotonic with adjustments */
		ns_to_ts(timer_monotonic(), ts);
		return 0;
	case CLOCK_MONOTONIC_COARSE:
		/* TODO(time): coarse (fast) monotonic with adjustments */
		ns_to_ts(timer_monotonic_coarse(), ts);
		return 0;
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

int
sc_gettid(void)
{
	return thread_id(thread_cur());
}
