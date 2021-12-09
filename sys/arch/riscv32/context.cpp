#include <arch/context.h>

#include <access.h>
#include <arch/interrupt.h>
#include <cassert>
#include <context.h>
#include <cpu.h>
#include <cstring>
#include <debug.h>
#include <kernel.h>
#include <locore.h>
#include <sch.h>
#include <sys/mman.h>
#include <thread.h>

constexpr auto kernel_xstatus =
#ifdef CONFIG_S_MODE
decltype(mstatus::SIE)(true).r;
#else
decltype(mstatus::MIE)(true).r;
#endif

/*
 * frame for signal delivery
 */
struct sigframe {
	ucontext_t uc;
	int rval;
};
static_assert(!(sizeof(sigframe) & 15));

/*
 * frame for real time signal delivery
 */
struct rt_sigframe {
	sigframe sf;
	siginfo_t si;
};
static_assert(!(sizeof(rt_sigframe) & 15));

/*
 * arch_schedule - call sch_switch as soon as possible
 */
void
arch_schedule()
{
	/* interrupts reschedule on return if necessary */
	if (interrupt_running())
		return;
	sch_switch();
}

/*
 * context_init_idle - initialise context for idle thread.
 *
 * This thread is special as it was initialised early in the boot process and
 * has an existing stack.
 */
void
context_init_idle(context *ctx, void *kstack_top)
{
	/* nothing to do */
}

/*
 * context_init_kthread - initialise context for kernel thread
 */
void
context_init_kthread(context *ctx, void *v_kstack_top, void (*entry)(void *),
		     void *arg)
{
	uintptr_t sp = reinterpret_cast<uintptr_t>(v_kstack_top);

	/* stack must be 128-bit aligned */
	assert(ALIGNEDn(sp, 16));

	/* setup stack for new kernel thread */
	struct stack {
		context_frame cf;
	};
	sp -= sizeof(stack);
	stack *s = reinterpret_cast<stack *>(sp);
	memset(s, 0, sizeof *s);
	s->cf.ra = reinterpret_cast<uint32_t>(thread_entry);
	s->cf.s[0] = reinterpret_cast<uint32_t>(entry);
	s->cf.s[1] = reinterpret_cast<uint32_t>(arg);
	s->cf.s[2] = kernel_xstatus;

	ctx->ksp = sp;
}

/*
 * context_init_uthread - initialise context for userspace thread
 */
int
context_init_uthread(context *child, as *as, void *v_kstack_top,
		     void *v_ustack_top, void (*entry)(), long rval)
{
	context *parent = &thread_cur()->ctx;

	/* stack layout for new userspace thread */
	struct stack {
		context_frame cf;
		trap_frame tf;
	};

	/* allocate a new stack frame */
	uintptr_t ksp = reinterpret_cast<uintptr_t>(v_kstack_top) - sizeof(stack);
	stack *s = reinterpret_cast<stack *>(ksp);
	memset(s, 0, sizeof *s);

	/* threads created by fork/vfork/clone don't specify an entry point and
	   must return to userspace as an exact clone of their parent */
	if (!entry) {
		/* copy trap frame from parent */
		memcpy(&s->tf,
		       reinterpret_cast<void *>(parent->kstack - sizeof(trap_frame)),
		       sizeof(trap_frame));
		/* if thread was created by vfork it shares stack with parent */
	} else {
		/* initialise trap frame */
		s->tf.xepc = reinterpret_cast<uint32_t>(entry);
		/* interrupts for higher privilege levels are always enabled */
		s->tf.xstatus = 0;
		s->tf.sp = reinterpret_cast<uint32_t>(v_ustack_top);
	}

	/* set syscall return value */
	s->tf.a[0] = rval;

	/* initialise context frame */
	s->cf.ra = reinterpret_cast<uint32_t>(thread_entry);
	s->cf.s[0] = reinterpret_cast<uint32_t>(return_to_user);
	s->cf.s[2] = kernel_xstatus;

	/* initialise context */
	child->ksp = ksp;
	child->kstack = reinterpret_cast<uintptr_t>(v_kstack_top);

	return 0;
}

/*
 * context_restore_vfork - restore context after vfork
 */
void
context_restore_vfork(context *ctx, as *as)
{
	/* nothing to do */
}

/*
 * context_set_signal - setup context for signal delivery
 */
bool
context_set_signal(context *ctx, const k_sigset_t *ss,
		   void (*handler)(int), void (*restorer)(), const int sig,
		   const siginfo_t *si, const int rval)
{
	/* get trap frame stored on entry to kernel in trap_entry */
	trap_frame *tf = reinterpret_cast<trap_frame *>(ctx->kstack - sizeof *tf);

	/* allocate stack frame for signal */
	uintptr_t usp = ALIGNn(tf->sp, 16);
	sigframe *ssf;
	siginfo_t *ssi = 0;
	if (si) {
		usp -= sizeof(rt_sigframe);
		ssi = reinterpret_cast<siginfo_t *>(usp + offsetof(rt_sigframe, si));
	} else usp -= sizeof(sigframe);
	ssf = reinterpret_cast<sigframe *>(usp);

	/* catch stack overflow */
	if (!u_access_ok(reinterpret_cast<void *>(usp), tf->sp - usp, PROT_WRITE))
		return DERR(false);

	/* initialise userspace signal context */
	memset(ssf, 0, sizeof(*ssf));
	ssf->uc.uc_mcontext.__gregs[0] = tf->xepc,
	ssf->uc.uc_mcontext.__gregs[1] = tf->ra,
	ssf->uc.uc_mcontext.__gregs[2] = tf->sp,
	ssf->uc.uc_mcontext.__gregs[3] = tf->gp,
	ssf->uc.uc_mcontext.__gregs[4] = tf->tp,
	ssf->uc.uc_mcontext.__gregs[5] = tf->t[0],
	ssf->uc.uc_mcontext.__gregs[6] = tf->t[1],
	ssf->uc.uc_mcontext.__gregs[7] = tf->t[2],
	ssf->uc.uc_mcontext.__gregs[8] = tf->s[0],
	ssf->uc.uc_mcontext.__gregs[9] = tf->s[1],
	ssf->uc.uc_mcontext.__gregs[10] = tf->a[0],
	ssf->uc.uc_mcontext.__gregs[11] = tf->a[1],
	ssf->uc.uc_mcontext.__gregs[12] = tf->a[2],
	ssf->uc.uc_mcontext.__gregs[13] = tf->a[3],
	ssf->uc.uc_mcontext.__gregs[14] = tf->a[4],
	ssf->uc.uc_mcontext.__gregs[15] = tf->a[5],
	ssf->uc.uc_mcontext.__gregs[16] = tf->a[6],
	ssf->uc.uc_mcontext.__gregs[17] = tf->a[7],
	ssf->uc.uc_mcontext.__gregs[18] = tf->s[2],
	ssf->uc.uc_mcontext.__gregs[19] = tf->s[3],
	ssf->uc.uc_mcontext.__gregs[20] = tf->s[4],
	ssf->uc.uc_mcontext.__gregs[21] = tf->s[5],
	ssf->uc.uc_mcontext.__gregs[22] = tf->s[6],
	ssf->uc.uc_mcontext.__gregs[23] = tf->s[7],
	ssf->uc.uc_mcontext.__gregs[24] = tf->s[8],
	ssf->uc.uc_mcontext.__gregs[25] = tf->s[9],
	ssf->uc.uc_mcontext.__gregs[26] = tf->s[10],
	ssf->uc.uc_mcontext.__gregs[27] = tf->s[11],
	ssf->uc.uc_mcontext.__gregs[28] = tf->t[3],
	ssf->uc.uc_mcontext.__gregs[29] = tf->t[4],
	ssf->uc.uc_mcontext.__gregs[30] = tf->t[5],
	ssf->uc.uc_mcontext.__gregs[31] = tf->t[6],
	ssf->uc.uc_sigmask.__bits[0] = ss->__bits[0];
	ssf->uc.uc_sigmask.__bits[1] = ss->__bits[1];
	if (si)
		*ssi = *si;
	ssf->rval = rval;

	/* adjust trap frame for signal delivery */
	tf->a[0] = sig;
	tf->a[1] = reinterpret_cast<uint32_t>(ssi);
	tf->a[2] = reinterpret_cast<uint32_t>(&ssf->uc);
	tf->ra = reinterpret_cast<uint32_t>(restorer);
	tf->xepc = reinterpret_cast<uint32_t>(handler);
	tf->sp = usp;

	return true;
}

/*
 * context_set_tls - set thread local storage pointer in context
 */
void
context_set_tls(context *ctx, void *tls)
{
	/* get trap frame */
	trap_frame *tf = reinterpret_cast<trap_frame *>(ctx->kstack - sizeof *tf);
	tf->tp = reinterpret_cast<uint32_t>(tls);
}

/*
 * context_restore - restore signal context
 */
bool
context_restore(context *ctx, k_sigset_t *ss, int *rval, bool siginfo)
{
	/* get trap frame stored on entry to kernel in trap_entry */
	trap_frame *tf = reinterpret_cast<trap_frame *>(ctx->kstack - sizeof *tf);

	/* get signal frame from user stack */
	const sigframe *sf = reinterpret_cast<const sigframe *>(tf->sp);

	/* check access to signal frame on user stack */
	if (!u_access_ok(sf, sizeof *sf, PROT_READ))
		return DERR(false);

	/* restore state */
	tf->xepc = sf->uc.uc_mcontext.__gregs[0];
	tf->ra = sf->uc.uc_mcontext.__gregs[1];
	tf->sp = sf->uc.uc_mcontext.__gregs[2];
	tf->gp = sf->uc.uc_mcontext.__gregs[3];
	tf->tp = sf->uc.uc_mcontext.__gregs[4];
	tf->t[0] = sf->uc.uc_mcontext.__gregs[5];
	tf->t[1] = sf->uc.uc_mcontext.__gregs[6];
	tf->t[2] = sf->uc.uc_mcontext.__gregs[7];
	tf->s[0] = sf->uc.uc_mcontext.__gregs[8];
	tf->s[1] = sf->uc.uc_mcontext.__gregs[9];
	tf->a[0] = sf->uc.uc_mcontext.__gregs[10];
	tf->a[1] = sf->uc.uc_mcontext.__gregs[11];
	tf->a[2] = sf->uc.uc_mcontext.__gregs[12];
	tf->a[3] = sf->uc.uc_mcontext.__gregs[13];
	tf->a[4] = sf->uc.uc_mcontext.__gregs[14];
	tf->a[5] = sf->uc.uc_mcontext.__gregs[15];
	tf->a[6] = sf->uc.uc_mcontext.__gregs[16];
	tf->a[7] = sf->uc.uc_mcontext.__gregs[17];
	tf->s[2] = sf->uc.uc_mcontext.__gregs[18];
	tf->s[3] = sf->uc.uc_mcontext.__gregs[19];
	tf->s[4] = sf->uc.uc_mcontext.__gregs[20];
	tf->s[5] = sf->uc.uc_mcontext.__gregs[21];
	tf->s[6] = sf->uc.uc_mcontext.__gregs[22];
	tf->s[7] = sf->uc.uc_mcontext.__gregs[23];
	tf->s[8] = sf->uc.uc_mcontext.__gregs[24];
	tf->s[9] = sf->uc.uc_mcontext.__gregs[25];
	tf->s[10] = sf->uc.uc_mcontext.__gregs[26];
	tf->s[11] = sf->uc.uc_mcontext.__gregs[27];
	tf->t[3] = sf->uc.uc_mcontext.__gregs[28];
	tf->t[4] = sf->uc.uc_mcontext.__gregs[29];
	tf->t[5] = sf->uc.uc_mcontext.__gregs[30];
	tf->t[6] = sf->uc.uc_mcontext.__gregs[31];
	ss->__bits[0] = sf->uc.uc_sigmask.__bits[0];
	ss->__bits[1] = sf->uc.uc_sigmask.__bits[1];

	*rval = sf->rval;

	return true;
}

/*
 * context_termiante - thread is terminating
 */
void
context_terminate(thread *th)
{
	/* nothing to do */
}

/*
 * context_free - free any dynamic memory allocated for context
 */
void
context_free(context *ctx)
{
	/* nothing to do */
}
