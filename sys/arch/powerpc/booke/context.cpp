#include <arch/context.h>

#include <arch/interrupt.h>
#include <booke/locore.h>
#include <cassert>
#include <context.h>
#include <debug.h>
#include <kernel.h>
#include <page.h>
#include <sch.h>

constexpr auto kernel_msr =
	decltype(MSR::CE)(true).r |
	decltype(MSR::EE)(true).r |
	decltype(MSR::PR)(MSR::ProblemState::Supervisor).r |
	decltype(MSR::ME)(true).r |
	decltype(MSR::DE)(true).r |
	decltype(MSR::IS)(0).r |
	decltype(MSR::DS)(0).r;

extern char _SDA_BASE_;
extern "C" void resched_from_thread();

/*
 * Call sch_switch as soon as possible
 */
void
arch_schedule()
{
	/* interrupts reschedule on return if necessary */
	if (interrupt_running())
		return;
	resched_from_thread();
}

/*
 * Initialise context for idle thread.
 *
 * This thread is special as it was initialised early in the boot process and
 * has an existing stack.
 */
void
context_init_idle(context *ctx, void *kstack_top)
{
	/* Boot thread starts with SPE enabled in case boot process uses SPE.
	   This makes sense as at boot there is no SPE context to restore.
	   We disable SPE now as the boot thread becomes the idle thread and
	   SPE should never be required for the idle thread.  This prevents
	   unnecessary saving & loading of SPE registers. */
	auto msr = mfmsr();
	msr.SPV = false;
	mtmsr(msr);
}

/*
 * Initialise context for kernel thread
 */
void
context_init_kthread(context *ctx, void *v_kstack_top, void (*entry)(void *),
		     void *arg)
{
	char *sp = reinterpret_cast<char *>(v_kstack_top);

	/* stack must be 16-byte aligned */
	assert(ALIGNEDn(sp, 16));

	/* setup stack for new kernel thread */
	struct stack {
		context_frame cf;
		min_frame mf;
	};
	sp -= sizeof(stack);
	stack *s = reinterpret_cast<stack *>(sp);
	memset(s, 0, sizeof(*s));
	s->cf.backchain = reinterpret_cast<uintptr_t>(&s->mf);
	s->cf.nip = reinterpret_cast<uintptr_t>(entry);
	s->cf.msr = kernel_msr;
	s->cf.r[1] = reinterpret_cast<uintptr_t>(&s->mf);
	s->cf.r[2] = 0;			    /* REVISIT: kernel r2? */
	s->cf.r[3] = reinterpret_cast<uintptr_t>(arg);
	s->cf.r[13] = reinterpret_cast<uintptr_t>(&_SDA_BASE_);
#if defined(POWER_CAT_SP)
	s->cf.spefscr = kernel_spefscr;
#endif

	ctx->ksp = sp;
}

/*
 * Initialise context for userspace thread
 */
int
context_init_uthread(context *child, as *as, void *kstack_top,
    void *ustack_top, void (*entry)(), long retval)
{
#warning context_init_uthread not implemented
	assert(0);
}

/*
 * Restore context after vfork
 */
void
context_restore_vfork(context *ctx, as *as)
{
#warning context_restore_vfork not implemented
	assert(0);
}

/*
 * Setup context for signal delivery
 */
bool
context_set_signal(context *ctx, const k_sigset_t *ss,
    void (*handler)(int), void (*restorer)(), const int sig,
    const siginfo_t *si, const int rval)
{
#warning context_set_signal not implemented
assert(0);
}

/*
 * Set thread local storage pointer in context
 */
void
context_set_tls(context *ctx, void *tls)
{
#warning context_set_tls not implemented
assert(0);
}

/*
 * Switch thread contexts
 */
void
context_switch(thread *prev, thread *next)
{
	/* context switch handled in locore.S */
}

/*
 * Restore context after signal delivery
 */
bool
context_restore(context *ctx, k_sigset_t *ss, int *rval, bool siginfo)
{
#warning context_restore not implemented
assert(0);
}

/*
 * Terminate thread context
 */
void
context_terminate(thread *th)
{
#warning context_terminate not implemented
assert(0);
}

/*
 * Free thread context
 */
void
context_free(context *ctx)
{
#warning context_free not implemented
assert(0);
}
