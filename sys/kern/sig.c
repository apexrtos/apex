#include <sig.h>

#include <access.h>
#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <irq.h>
#include <proc.h>
#include <sch.h>
#include <sections.h>
#include <string.h>
#include <sys/mman.h>
#include <task.h>
#include <thread.h>

typedef void (*sig_restore_fn)(void);
typedef void (*sig_handler_fn)(int);

#define trace(...)

/*
 * Kernel signal set maniuplation
 */
static void
ksigandnset(k_sigset_t *os, const k_sigset_t *ls, const k_sigset_t *rs)
{
	unsigned long *o = os->__bits;
	const unsigned long *l = ls->__bits, *r = rs->__bits;
	switch (NSIG / 8 / sizeof(long)) {
	case 4: o[3] = l[3] & ~r[3];
		o[2] = l[2] & ~r[2];
	case 2: o[1] = l[1] & ~r[1];
	case 1: o[0] = l[0] & ~r[0];
	}
}

static void
ksigorset(k_sigset_t *os, const k_sigset_t *ls, const k_sigset_t *rs)
{
	unsigned long *o = os->__bits;
	const unsigned long *l = ls->__bits, *r = rs->__bits;
	switch (NSIG / 8 / sizeof(long)) {
	case 4: o[3] = l[3] | r[3];
		o[2] = l[2] | r[2];
	case 2: o[1] = l[1] | r[1];
	case 1: o[0] = l[0] | r[0];
	}
}

static void
ksigaddset(k_sigset_t *os, int sig)
{
	const unsigned n = sig - 1;
	assert(n < NSIG);
	os->__bits[n / 8 / sizeof(long)] |= 1UL << (n & (8 * sizeof(long) - 1));
}

static void
ksigdelset(k_sigset_t *os, int sig)
{
	const unsigned n = sig - 1;
	assert(n < NSIG);
	os->__bits[n / 8 / sizeof(long)] &= ~(1UL << (n & (8 * sizeof(long) - 1)));
}

static int
ksigfirst(const k_sigset_t *ss)
{
	const unsigned long *s = ss->__bits;
	switch (NSIG / 8 / sizeof(long)) {
	case 4: return __builtin_ffsl(s[0]) ?:
			8 * sizeof(long) + __builtin_ffsl(s[1]) ?:
			16 * sizeof(long) + __builtin_ffsl(s[2]) ?:
			24 * sizeof(long) + __builtin_ffsl(s[3]);
	case 2: return __builtin_ffsl(s[0]) ?:
			8 * sizeof(long) + __builtin_ffsl(s[1]);
	case 1: return __builtin_ffsl(s[0]);
	}
}

static bool
ksigisemptyset(const k_sigset_t *ss)
{
	const unsigned long *s = ss->__bits;
	switch (NSIG / 8 / sizeof(long)) {
	case 4: return !s[0] && !s[1] && !s[2] && !s[3];
	case 2: return !s[0] && !s[1];
	case 1: return !s[0];
	}
}

static void
ksigallset(k_sigset_t *ss)
{
	unsigned long *s = ss->__bits;
	switch (NSIG / 8 / sizeof(long)) {
	case 4: s[3] = -1;
		s[2] = -1;
	case 2: s[1] = -1;
	case 1: s[0] = -1;
	}
}

/*
 * Check if signal is ignored
 */
static bool
sig_ignore(void *handler, int sig)
{
	if (handler == SIG_IGN)
		return true;

	/* default action for these signals is to ignore */
	switch (sig) {
	case SIGCONT:
	case SIGCHLD:
	case SIGWINCH:
	case SIGURG:
		return handler == SIG_DFL;
	}

	return false;
}

/*
 * Get handler for signal
 */
static void *
sig_handler(struct task *t, int sig)
{
	assert(sig > 0 && sig <= NSIG);
	return t->sig_action[sig - 1].handler;
}

/*
 * Get flags for signal
 */
static unsigned long
sig_flags(struct task *t, int sig)
{
	assert(sig > 0 && sig <= NSIG);
	return t->sig_action[sig - 1].flags;
}

/*
 * Get context restore trampoline for signal
 */
static sig_restore_fn
sig_restorer(struct task *t, int sig)
{
	assert(sig > 0 && sig <= NSIG);
	return sig_flags(t, sig) & SA_RESTORER ? t->sig_action[sig - 1].restorer : 0;
}

/*
 * Get signal mask for signal
 */
static k_sigset_t*
sig_mask(struct task *t, int sig)
{
	assert(sig > 0 && sig <= NSIG);
	return &t->sig_action[sig - 1].mask;
}

/*
 * Try to find threads to handle signals sent to task
 *
 * Call with scheduler locked
 *
 * Can be called under interrupt
 */
static void
sig_flush(struct task *task)
{
	if (ksigisemptyset(&task->sig_pending))
		return;

	struct thread *th;
	list_for_each_entry(th, &task->threads, task_link) {
		k_sigset_t unblocked;
		ksigandnset(&unblocked, &task->sig_pending, &th->sig_blocked);

		if (ksigisemptyset(&unblocked))
			continue;
		/*
		 * Thread can handle one or more signals
		 * Mark signals as pending on thread
		 */
		const int s = irq_disable();
		ksigorset(&th->sig_pending, &th->sig_pending, &unblocked);
		irq_restore(s);

		/*
		 * Wake up thread to handle signals
		 */
		sch_signal(th);

		/*
		 * Clear pending signals from task
		 */
		ksigandnset(&task->sig_pending, &task->sig_pending, &unblocked);

		/*
		 * Quit if no more signals
		 */
		if (ksigisemptyset(&task->sig_pending))
			break;
	}
}

/*
 * Handle job control signals (SIGSTOP, SIGCONT, SIGKILL).
 * Handle ignored signals.
 *
 * Returns
 * -ve : error
 * 0   : continue processing signal
 * +ve : ignore signal
 *
 * Call with scheduler locked
 */
static int
process_signal(struct task *task, int sig)
{
	int ret;

	switch (sig) {
	case SIGSTOP:
		if ((ret = task_suspend(task)) < 0)
			return ret;
		return 1;   /* always ignore */
	case SIGCONT:
		if ((ret = task_resume(task)) < 0)
			return ret;
		break;	    /* potentially process */
	case SIGKILL:
		proc_exit(task, 0, sig);
		return 1;   /* always ignore */
	}

	/*
	 * Check if signal is ignored by task
	 */
	if (sig_ignore(sig_handler(task, sig), sig))
		return 1;   /* ignore */
	return 0;	    /* process */
}

/*
 * Signal a task
 *
 * Can be called under interrupt
 *
 * If sig == 0 no signal is sent but error checking is still performed.
 */
int
sig_task(struct task *task, int sig)
{
	trace("sig_task sig:%d\n", sig);

	if (!task_valid(task))
		return DERR(-EINVAL);
	if (sig < 0 || sig > NSIG)
		return DERR(-EINVAL);
	if (task == &kern_task)
		return DERR(-ESRCH);
	if (sig == 0)
		return 0;

	int ret;

	sch_lock();

	ret = process_signal(task, sig);
	if (ret < 0) {
		/* Error */
		goto out;
	}
	if (ret > 0) {
		/* Ignore signal */
		ret = 0;
		goto out;
	}

	/*
	 * Mark signal as pending on task
	 */
	const int s = irq_disable();
	ksigaddset(&task->sig_pending, sig);
	irq_restore(s);

	/*
	 * Signal a thread if possible
	 */
	sig_flush(task);

out:
	sch_unlock();
	return ret;
}

/*
 * Signal a specific thread
 *
 * Can be called under interrupt
 *
 * Signal 0 is a special signal sent to kill a thread
 */
void
sig_thread(struct thread *th, int sig)
{
	trace("sig_thread th:%p sig:%d\n", th, sig);

	assert(sig >= 0 && sig <= NSIG);

	int ret;

	sch_lock();

	if (sig) {
		ret = process_signal(th->task, sig);
		if (ret < 0) {
			/* Error */
			goto out;
		}
		if (ret > 0) {
			/* Ignore signal */
			ret = 0;
			goto out;
		}
	}

	/*
	 * Mark signal as pending on thread
	 */
	const int s = irq_disable();
	ksigaddset(&th->sig_pending, sig ?: SIGKILL);
	irq_restore(s);

	const bool unblocked_pending = sig_unblocked_pending(th);

	/*
	 * Interrupt thread to handle unblocked pending signal
	 */
	if (unblocked_pending)
		sch_signal(th);

out:
	sch_unlock();
}

bool
sig_unblocked_pending(struct thread *th)
{
	k_sigset_t unblocked;
	ksigandnset(&unblocked, &th->sig_pending, &th->sig_blocked);
	return !ksigisemptyset(&unblocked);
}

/*
 * sig_block_all - block all signals for thread, return old signal mask
 */
k_sigset_t
sig_block_all()
{
	sch_lock();
	const k_sigset_t old = thread_cur()->sig_blocked;
	ksigallset(&thread_cur()->sig_blocked);
	sch_unlock();
	return old;
}

/*
 * sig_restore - restore signal mask
 */
void
sig_restore(const k_sigset_t *old)
{
	struct thread *th = thread_cur();
	sch_lock();
	thread_cur()->sig_blocked = *old;
	if (sig_unblocked_pending(th)) {
		/*
		 * Wake up thread to handle signal
		 */
		sch_signal(th);
	}
	sch_unlock();
}

/*
 * Adjust signal handlers after exec call
 */
void
sig_exec(struct task *t)
{
	/*
	 * http://pubs.opengroup.org/onlinepubs/009695399/functions/exec.html
	 *
	 * Signals set to the default action (SIG_DFL) in the calling process
	 * image shall be set to the default action in the new process image.
	 *
	 * Except for SIGCHLD, signals set to be ignored (SIG_IGN) by the
	 * calling process image shall be set to be ignored by the new process
	 * image.
	 *
	 * Signals set to be caught by the calling process image shall be set
	 * to the default action in the new process image (see <signal.h>).
	 *
	 * If the SIGCHLD signal is set to be ignored by the calling process
	 * image, it is unspecified whether the SIGCHLD signal is set to be
	 * ignored or to the default action in the new process image.
	 */
	for (size_t i = 1; i <= NSIG; ++i) {
		const sig_handler_fn h = sig_handler(t, i);
		if (h == SIG_DFL || h == SIG_IGN)
			continue;
		t->sig_action[i - 1].handler = SIG_DFL;
	}
}

/*
 * Deliver pending signals to current thread
 *
 * If a syscall was running sc_rval is the return value of the interrupted
 * syscall. 0 otherwise.
 *
 * Returns syscall return value if returning from syscall or signal number
 * if delivering signal.
 *
 * Can be called under interrupt.
 */
static int __attribute__((noinline))
sig_deliver_slowpath(k_sigset_t pending, int rval)
{
	/*
	 * Thread is terminating
	 */
	if (sch_testexit())
		return -ETHREAD_EXIT;

	struct thread *th = thread_cur();
	struct task *task = task_cur();

	sch_lock();

	/*
	 * Any unblocked pending signals?
	 */
	k_sigset_t unblocked;
	ksigandnset(&unblocked, &pending, &th->sig_blocked);
	if (ksigisemptyset(&unblocked))
		goto out;

	const int sig = ksigfirst(&unblocked);
	const sig_handler_fn handler = sig_handler(task, sig);

	/*
	 * Clear unblocked signal
	 */
	const int s1 = irq_disable();
	ksigdelset(&th->sig_pending, sig);
	irq_restore(s1);

	/*
	 * Ignored signals are filtered out earlier
	 */
	assert(handler != SIG_IGN);

	if (handler != SIG_DFL) {
		trace("Delivering signal th:%p sig:%d\n", th, sig);

		/*
		 * If a syscall was interrupted and the signal flags include
		 * SA_RESTART we need to restart the syscall after the signal
		 * handler returns.
		 *
		 * rval will be returned from sc_sigreturn and sc_rt_sigreturn
		 */
		if (rval == -EINTR && sig_flags(task, sig) & SA_RESTART)
			rval = -ERESTARTSYS;
		else if (rval == -EINTR_NORESTART)
			rval = -EINTR;

		siginfo_t si;
		const bool info = sig_flags(task, sig) & SA_SIGINFO;

		if (info) {
			/* TODO: more to fill in here */
			si = (siginfo_t){
				.si_signo = sig,
			};
		}

		/* setup context to run signal handler */
		if (!context_set_signal(&th->ctx, &th->sig_blocked, handler,
		    sig_restorer(task, sig), sig, info ? &si : 0, rval)) {
			dbg("Signal setup failed. Terminate.\n");
			goto fatal;
		}

		/* adjust blocked signal mask */
		ksigorset(&th->sig_blocked, &th->sig_blocked, sig_mask(task, sig));
		if (!(sig_flags(task, sig) & SA_NODEFER))
			ksigaddset(&th->sig_blocked, sig);

		/* SIGSTOP and SIGKILL cannot be blocked */
		ksigdelset(&th->sig_blocked, SIGSTOP);
		ksigdelset(&th->sig_blocked, SIGKILL);

		rval = sig;
		goto out;
	}

	/*
	 * Default action
	 */
	switch (sig) {
	case SIGQUIT:
	case SIGILL:
	case SIGABRT:
	case SIGFPE:
	case SIGSEGV:
	case SIGBUS:
	case SIGSYS:
	case SIGTRAP:
	case SIGXCPU:
	case SIGXFSZ:
		/* Default action: dump core & terminate */
		/* XXX: actually dump core for these one day */
	case SIGHUP:
	case SIGINT:
	case SIGPIPE:
	case SIGALRM:
	case SIGTERM:
	case SIGUSR1:
	case SIGUSR2:
	case SIGPOLL:
	case SIGPROF:
	case SIGVTALRM:
	case SIGSTKFLT:
	case SIGPWR:
	case 32 ... NSIG:	/* real time signals */
		/* Default action: terminate */
		dbg("Fatal signal %d. Terminate.\n", sig);
fatal:
		proc_exit(task_cur(), 0, sig);
		sch_unlock();
		sch_testexit();
		return -ETHREAD_EXIT;
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		/* Default action: stop */
		task_suspend(task);
		goto out;
	case SIGKILL:
	case SIGSTOP:
		/* Always handled by process_signal */
	case SIGWINCH:
	case SIGCHLD:
	case SIGURG:
	case SIGCONT:
		/* Default action: ignore, but we have no handler?? */
		assert(0);
	}

out:
	sch_unlock();
	return rval;
}

__fast_text int
sig_deliver(int rval)
{
	struct thread *th = thread_cur();
	const k_sigset_t pending = th->sig_pending;

	/*
	 * Any pending signals?
	 */
	if (!ksigisemptyset(&pending))
		rval = sig_deliver_slowpath(pending, rval);

	/*
	 * Returning to userspace with a locked kernel mutex is a bug.
	 */
#if defined(CONFIG_DEBUG)
	assert(!th->mutex_locks);
	assert(!th->spinlock_locks);
	assert(!th->rwlock_locks);
	assert(!sch_locks());
#endif

	return rval;
}

/*
 * Set signal mask for current thread
 */
int
sc_rt_sigprocmask(int how, const k_sigset_t *uset, k_sigset_t *uoldset,
    const size_t size)
{
	trace("sc_rt_sigprocmask how:%d uset:%p uoldset:%p size:%zu\n",
	    how, uset, uoldset, size);

	if (sizeof(k_sigset_t) != size)
		return DERR(-EINVAL);

	int ret;

	if ((ret = u_access_begin()) < 0)
		return ret;

	sch_lock();

	if (uoldset) {
		if (!u_access_ok(uoldset, sizeof *uoldset, PROT_WRITE)) {
			ret = DERR(-EFAULT);
			goto out;
		}
		memcpy(uoldset, &thread_cur()->sig_blocked, sizeof(k_sigset_t));
	}

	if (!uset)
		goto out;
	if (!u_access_ok(uset, sizeof *uset, PROT_READ)) {
		ret = DERR(-EFAULT);
		goto out;
	}

	switch (how) {
	case SIG_BLOCK:
		ksigorset(&thread_cur()->sig_blocked, &thread_cur()->sig_blocked,
		    (k_sigset_t *)uset);
		break;
	case SIG_UNBLOCK:
		ksigandnset(&thread_cur()->sig_blocked, &thread_cur()->sig_blocked,
		    (k_sigset_t *)uset);
		break;
	case SIG_SETMASK:
		memcpy(&thread_cur()->sig_blocked, uset, sizeof(k_sigset_t));
		break;
	default:
		ret = DERR(-EINVAL);
		goto out;
	}

	/* SIGSTOP and SIGKILL cannot be blocked */
	ksigdelset(&thread_cur()->sig_blocked, SIGSTOP);
	ksigdelset(&thread_cur()->sig_blocked, SIGKILL);

	/*
	 * Some pending task signals may now be unblocked
	 */
	sig_flush(task_cur());

out:
	sch_unlock();
	u_access_end();
	return ret;
}

/*
 * Set signal action for current task
 */
int
sc_rt_sigaction(const int sig, const struct k_sigaction *uact,
    struct k_sigaction *uoldact, size_t size)
{
	trace("rt_sigaction sig:%d uact:%p uoldact:%p size:%zu\n",
	    sig, uact, uoldact, size);

	if (sig > NSIG || sig < 1)
		return DERR(-EINVAL);

	int ret;

	if ((ret = u_access_begin()) < 0)
		return ret;

	sch_lock();

	if (uoldact) {
		if (!u_access_ok(uoldact, sizeof *uoldact, PROT_WRITE)) {
			ret = DERR(-EFAULT);
			goto out;
		}
		memcpy(uoldact, &task_cur()->sig_action[sig - 1],
		    sizeof(struct k_sigaction));
	}

	if (!uact)
		goto out;

	/*
	 * SIGSTOP and SIGKILL cannot be caught or ignored
	 * - sigaction must succeed if uact == NULL even for invalid signals
	 */
	if (sig == SIGKILL || sig == SIGSTOP) {
		ret = DERR(-EINVAL);
		goto out;
	}

	if (!u_access_ok(uact, sizeof *uact, PROT_READ)) {
		ret = DERR(-EFAULT);
		goto out;
	}

	struct k_sigaction kact = *uact;

	if (sizeof(kact.mask) != size) {
		ret = DERR(-EINVAL);
		goto out;
	}

	trace("rt_sigaction flags %lx\n", kact.flags);

	/* Apex only supports limited flags */
	if ((kact.flags & (SA_RESTORER | SA_RESTART | SA_NODEFER | SA_SIGINFO)) !=
	    kact.flags) {
		ret = DERR(-ENOSYS);
		goto out;
	}

	task_cur()->sig_action[sig - 1] = kact;

	/*
	 * http://pubs.opengroup.org/onlinepubs/007908775/xsh/sigaction.html
	 *
	 * Setting a signal action to SIG_DFL for a signal that is pending, and
	 * whose default action is to ignore the signal (for example, SIGCHLD),
	 * will cause the pending signal to be discarded, whether or not it is
	 * blocked.
	 *
	 * Setting a signal action to SIG_IGN for a signal that is pending will
	 * cause the pending signal to be discarded, whether or not it is
	 * blocked.
	 */
	if (sig_ignore(sig_handler(task_cur(), sig), sig)) {
		struct thread *th;
		const int s = irq_disable();
		list_for_each_entry(th, &task_cur()->threads, task_link) {
			ksigdelset(&th->sig_pending, sig);
		}
		ksigdelset(&task_cur()->sig_pending, sig);
		irq_restore(s);
	}

out:
	sch_unlock();
	u_access_end();
	return ret;
}

/*
 * Return from signal handler
 */
static int
sigreturn(bool siginfo)
{
	int ret;
	struct thread *th = thread_cur();

	sch_lock();
	if (!context_restore(&th->ctx, &th->sig_blocked, &ret, siginfo)) {
		proc_exit(task_cur(), 0, SIGSYS);
		sch_unlock();
		sch_testexit();
		return -ETHREAD_EXIT;
	}

	/* SIGSTOP and SIGKILL cannot be blocked */
	ksigdelset(&th->sig_blocked, SIGSTOP);
	ksigdelset(&th->sig_blocked, SIGKILL);

	sch_unlock();

	return ret;
}

/*
 * Return from signal handler (1 argument)
 */
int
sc_sigreturn(void)
{
	return sigreturn(false);

}

/*
 * Return from SA_SIGINFO signal handler (3 arguments)
 */
int
sc_rt_sigreturn(void)
{
	return sigreturn(true);
}
