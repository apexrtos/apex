#include <syscalls.h>

#include <access.h>
#include <algorithm>
#include <arch/machine.h>
#include <compiler.h>
#include <cstring>
#include <ctime>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <proc.h>
#include <sch.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <task.h>
#include <thread.h>
#include <time32.h>
#include <version.h>
#include <vm.h>

void
sc_exit()
{
	thread *th = thread_cur();

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
	proc_exit(task_cur(), status, 0);
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
	strcpy(u->sysname, "Apex RTOS");
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

	switch ((unsigned)cmd) {
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
	proc_exit(task_cur(), 0, 0);
	return 0;
}

int
sc_nanosleep(const timespec32 *req, timespec32 *rem)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_access_ok(req, sizeof *req, PROT_READ))
		return DERR(-EFAULT);
	const uint_fast64_t ns = ts32_to_ns(*req);
	if (!ns)
		return 0;
	l.unlock();
	const uint_fast64_t r = timer_delay(ns);
	if (!r)
		return 0;
	if (rem) {
		if (auto r = l.lock(); r < 0)
			return r;
		if (!u_access_ok(rem, sizeof *rem, PROT_WRITE))
			return DERR(-EFAULT);
		*rem = ns_to_ts32(r);
	}
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
		*ts = ns_to_ts(timer_realtime());
		return 0;
	case CLOCK_REALTIME_COARSE:
		*ts = ns_to_ts(timer_realtime_coarse());
		return 0;
	case CLOCK_MONOTONIC:
		/* TODO(time): monotonic with adjustments */
		*ts = ns_to_ts(timer_monotonic());
		return 0;
	case CLOCK_MONOTONIC_COARSE:
		/* TODO(time): coarse (fast) monotonic with adjustments */
		*ts = ns_to_ts(timer_monotonic_coarse());
		return 0;
	case CLOCK_MONOTONIC_RAW:
		/* TODO(time): monotonic without adjustments */
		*ts = ns_to_ts(timer_monotonic());
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
sc_clock_nanosleep(clockid_t id, int flags, const struct timespec *req,
		   struct timespec *rem)
{
	if (flags || id != CLOCK_REALTIME)
		return DERR(-ENOTSUP);

	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_access_ok(req, sizeof *req, PROT_READ))
		return DERR(-EFAULT);
	const uint_fast64_t ns = ts_to_ns(*req);
	if (!ns)
		return 0;
	l.unlock();
	const uint_fast64_t r = timer_delay(ns);
	if (!r)
		return 0;
	if (rem) {
		if (auto r = l.lock(); r < 0)
			return r;
		if (!u_access_ok(rem, sizeof *rem, PROT_WRITE))
			return DERR(-EFAULT);
		*rem = ns_to_ts(r);
	}
	return -EINTR_NORESTART;
}

int sc_clock_settime(clockid_t id, const timespec *ts)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_access_ok(ts, sizeof *ts, PROT_READ))
		return DERR(-EFAULT);
	switch (id) {
	case CLOCK_REALTIME:
		return timer_realtime_set(ts_to_ns(*ts));
	default:
		return DERR(-EINVAL);
	}
}

int sc_clock_settime32(clockid_t id, const timespec32 *ts)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_access_ok(ts, sizeof *ts, PROT_READ))
		return DERR(-EFAULT);
	switch (id) {
	case CLOCK_REALTIME:
		return timer_realtime_set(ts32_to_ns(*ts));
	default:
		return DERR(-EINVAL);
	}
}

int
sc_gettid()
{
	return thread_id(thread_cur());
}

int
sc_sched_getparam(int id, sched_param *param)
{
	interruptible_lock ul{u_access_lock};
	if (auto r = ul.lock(); r < 0)
		return r;
	if (!u_access_ok(param, sizeof *param, PROT_WRITE) ||
	    !ALIGNED(param, sched_param))
		return DERR(-EFAULT);

	/* take a spinlock to disable preemption so that thread remains valid */
	/* REVISIT(SMP): this will need to be rewritten */
	a::spinlock sl;
	std::lock_guard pl{sl};

	thread *th{id ? thread_find(id) : thread_cur()};
	if (!th)
		return DERR(-ESRCH);

	param->sched_priority = sch_getprio(th);
	return 0;
}

int
sc_sched_getscheduler(int id)
{
	/* take a spinlock to disable preemption so that thread remains valid */
	/* REVISIT(SMP): this will need to be rewritten */
	a::spinlock sl;
	std::lock_guard pl{sl};

	thread *th{id ? thread_find(id) : thread_cur()};
	if (!th)
		return DERR(-ESRCH);

	return sch_getpolicy(th);
}

int
sc_sched_setscheduler(int id, int policy, const sched_param *param)
{
	using std::min;

	interruptible_lock ul{u_access_lock};
	if (auto r = ul.lock(); r < 0)
		return r;
	if (!u_access_ok(param, sizeof *param, PROT_READ) ||
	    !ALIGNED(param, sched_param))
		return DERR(-EFAULT);

	const auto prio{read_once(&param->sched_priority)};
	const auto prio_min{sched_get_priority_min(policy)};
	const auto prio_max{sched_get_priority_max(policy)};
	if (prio_min < 0)
		return prio_min;
	if (prio_max < 0)
		return prio_max;
	if (prio > prio_min || prio < prio_max)
		return DERR(-EINVAL);

	/* take a spinlock to disable preemption so that thread remains valid */
	/* REVISIT(SMP): this will need to be rewritten */
	a::spinlock sl;
	std::lock_guard pl{sl};

	thread *th{id ? thread_find(id) : thread_cur()};
	if (!th)
		return DERR(-ESRCH);

	if (auto r{sch_setpolicy(th, policy)}; r < 0)
		return r;
	sch_setprio(th, prio, min(prio, sch_getprio(th)));
	return 0;
}
