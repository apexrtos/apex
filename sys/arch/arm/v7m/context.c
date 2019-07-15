#include <arch.h>

#include <cpu.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <thread.h>

/*
 * uthread_entry
 *
 * Entry point for userspace threads.
 *
 * Userspace threads start with interrupts enabled.
 */
__attribute__((naked)) static void
uthread_entry(void)
{
	asm(
		"cpsie i\n"
		"b asm_extended_syscall_ret\n"
	);
}

/*
 * kthread_entry
 *
 * Entry point for kernel threads.
 * r0 = arg
 * r1 = entry
 *
 * Kernel threads start with interrupts enabled.
 */
__attribute__((naked)) static void
kthread_entry(void)
{
	asm(
		"cpsie i\n"
		"bx r1\n"
	);
}

/*
 * Initialise context for idle thread.
 *
 * This thread is special as it was initialised early in the boot process and
 * has an existing stack.
 */
void
context_init_idle(struct context *ctx, void *kstack_top)
{
	/* nothing to do */
}

/*
 * Initialise context for kernel thread
 */
void
context_init_kthread(struct context *ctx, void *kstack_top,
    void (*entry)(void *), void *arg)
{
	/* stack must be 8-byte aligned */
	assert(!((uint32_t)kstack_top & 7));

	/* allocate a new kernel thread frame */
	kstack_top -= sizeof(struct kthread_frame);
	struct kthread_frame *ktframe = kstack_top;

	/* set thread arguments */
	ktframe->eframe.r0 = (uint32_t)arg;
	ktframe->eframe.r1 = (uint32_t)entry;

	/* Loading an unaligned value from the stack into the PC on an
	   exception return is UNPREDICTABLE. */
	ktframe->eframe.ra = (uint32_t)kthread_entry & -2;
	ktframe->eframe.xpsr = EPSR_T | 11;

	/* Pop exception frame after context switch */
	ktframe->nvregs.lr = EXC_RETURN_KERN;

	/* initialise kernel registers */
	ctx->kregs.msp = (uint32_t)kstack_top;
	ctx->kregs.shcsr = (union scb_shcsr){.SVCALLACT = 1}.r;
}

/*
 * Initialise context for userspace thread
 */
void
context_init_uthread(struct context *ctx, void *kstack_top,
    void *ustack_top, void (*entry)(void), long retval)
{
	bool shared_stack = false;

	/* if thread was created by vfork it shares stack with parent */
	if (!ustack_top) {
		shared_stack = true;
		asm("mrs %0, psp" : "=r" (ustack_top));
	}
	assert(ustack_top);

	/* stack must be 8-byte aligned */
	assert(!((uint32_t)kstack_top & 7));
	assert(!((uint32_t)ustack_top & 7));

	/* allocate a new thread frame */
	kstack_top -= sizeof(struct uthread_frame);
	struct uthread_frame *utframe = kstack_top;

	/* threads created by fork/vfork/clone don't specify an entry point and
	 * must return to userspace as an exact clone of their creator */
	if (!entry) {
		/* copy extended syscall frame */
		struct extended_syscall_frame *cur_esframe =
		    arch_kstack_align(thread_cur()->kstack + CONFIG_KSTACK_SIZE) -
		    sizeof(struct extended_syscall_frame);
		utframe->esframe = *cur_esframe;

		/* copy tls pointer */
		ctx->tls = thread_cur()->ctx.tls;

		if (!shared_stack) {
			/* copy return-to-userspace exception frame */
			ustack_top -= sizeof(struct exception_frame);
			struct exception_frame *eframe = ustack_top;

			struct exception_frame *cur_eframe;
			asm("mrs %0, psp" : "=r" (cur_eframe));
			*eframe = *cur_eframe;
		}
	} else {
		assert(!shared_stack);

		/* allocate a new exception frame for return to userspace */
		ustack_top -= sizeof(struct exception_frame);
		struct exception_frame *eframe = ustack_top;

		/* Loading an unaligned value from the stack into the PC on an
		   exception return is UNPREDICTABLE. */
		eframe->ra = (uint32_t)entry & -2;
		eframe->xpsr = EPSR_T;
	}

	/* set thread arguments */
	utframe->eframe.r0 = retval;

	/* Loading an unaligned value from the stack into the PC on an
	   exception return is UNPREDICTABLE. */
	utframe->eframe.ra = (uint32_t)uthread_entry & -2;
	utframe->eframe.xpsr = EPSR_T | 11;

	/* Pop exception frame after context switch */
	utframe->nvregs.lr = EXC_RETURN_KERN;

	/* initialise kernel registers */
	ctx->kregs.msp = (uint32_t)kstack_top;
	ctx->kregs.psp = (uint32_t)ustack_top;
	ctx->kregs.shcsr = (union scb_shcsr){.SVCALLACT = 1}.r;
}

/*
 * Allocate stack space for SA_SIGINFO data
 */
void
context_alloc_siginfo(struct context *ctx, siginfo_t **si, ucontext_t **uc)
{
	void *sp;
	asm("mrs %0, psp" : "=r" (sp));

	/* can't signal kernel thread! */
	if (!sp)
		panic("signal kthread");

	assert(!ctx->saved_psp);
	ctx->saved_psp = sp;

	sp -= sizeof(ucontext_t);
	*uc = sp;
	sp -= sizeof(siginfo_t);
	*si = sp;
	static_assert(!((sizeof(ucontext_t) + sizeof(siginfo_t)) & 7), "");

	asm("msr psp, %0" :: "r" (sp));
}

/*
 * Fill in ucontext
 */
void
context_init_ucontext(struct context *ctx, const k_sigset_t *ss, ucontext_t *uc)
{
	assert(ctx->saved_psp);
	const struct exception_frame *eframe = ctx->saved_psp;

	/*
	 * This is as much context as we can supply using just an exception
	 * frame. For musl it is enough (pthread_cancel).
	 */
	*uc = (ucontext_t){
		.uc_flags = 0,
		.uc_link = 0,
		.uc_stack = (stack_t){},
		.uc_mcontext = (mcontext_t){
			.arm_r0 = eframe->r0,
			.arm_r1 = eframe->r1,
			.arm_r2 = eframe->r2,
			.arm_r3 = eframe->r3,
			.arm_ip = eframe->r12,
			.arm_lr = eframe->lr,
			.arm_pc = eframe->ra,
			.arm_cpsr = eframe->xpsr,
		},
		.uc_sigmask = (sigset_t){
			.__bits[0] = ss->__bits[0],
			.__bits[1] = ss->__bits[1],
		},
		.uc_regspace = {},
	};
}

/*
 * Stack frame for signal delivery
 */
struct signal_frame {
	struct exception_frame eframe;
	struct k_sigset_t saved_sigset;
	struct syscall_frame saved_sframe;
	int saved_rrval;
	uint32_t dummy;			    /* stack must be 8-byte aligned */
};
static_assert(!(sizeof(struct signal_frame) & 7), "");

/*
 * Setup context for signal delivery
 */
void
context_set_signal(struct context *ctx, const k_sigset_t *ss,
    void (*handler)(int), void (*restorer)(void), const int sig,
    const siginfo_t *si, const ucontext_t *uc, const int rrval)
{
	void *sp;
	asm("mrs %0, psp" : "=r" (sp));

	/* can't signal kernel thread! */
	if (!sp)
		panic("signal kthread");

	/* psp already saved if context_alloc_siginfo */
	if (!ctx->saved_psp)
		ctx->saved_psp = sp;

	/* build new stack frame for signal */
	struct signal_frame *sf = sp - sizeof(struct signal_frame);

	asm("msr psp, %0" :: "r" (sf));

	sf->eframe = (struct exception_frame){
		.r0 = sig,
		.r1 = (uint32_t)si,
		.r2 = (uint32_t)uc,
		.lr = (uint32_t)restorer,
		.ra = (uint32_t)handler & -2,
		.xpsr = EPSR_T,
	};
	if (ss)
		sf->saved_sigset = *ss;
	sf->saved_rrval = rrval;
	if (rrval == -ERESTARTSYS || rrval == -EPENDSV_RETURN) {
		const struct syscall_frame *sframe =
		    arch_kstack_align(thread_cur()->kstack + CONFIG_KSTACK_SIZE) -
		    sizeof(struct syscall_frame);
		sf->saved_sframe = *sframe;
	}
}

/*
 * Set thread local storage pointer in context
 */
void
context_set_tls(struct context *ctx, void *tls)
{
	ctx->tls = tls;
}

/*
 * Return true if context is already setup for signal delivery
 */
bool
context_in_signal(struct context *ctx)
{
	return ctx->saved_psp;
}

/*
 * Restore context after signal return
 */
int
context_restore(struct context *ctx, k_sigset_t *ss)
{
	if (!ctx->saved_psp)
		return DERR(-EINVAL);

	/* get signal frame */
	struct signal_frame *sf;
	asm("mrs %0, psp" : "=r" (sf));

	/* restore stack pointer */
	asm("msr psp, %0" :: "r" (ctx->saved_psp));
	ctx->saved_psp = 0;

	/* restore state */
	if (ss)
		*ss = sf->saved_sigset;
	if (sf->saved_rrval == -ERESTARTSYS ||
	    sf->saved_rrval == -EPENDSV_RETURN) {
		struct syscall_frame *sframe =
		    arch_kstack_align(thread_cur()->kstack + CONFIG_KSTACK_SIZE) -
		    sizeof(struct syscall_frame);
		*sframe = sf->saved_sframe;
	}
	return sf->saved_rrval;
}

/*
 * Restore extended context after signal return
 */
int
context_siginfo_restore(struct context *ctx, k_sigset_t *ss)
{
	if (!ctx->saved_psp)
		return DERR(-EINVAL);

	/* retrieve ucontext */
	ucontext_t *uc = ctx->saved_psp - sizeof(ucontext_t);

	/* update exception frame in case ucontext was changed */
	struct exception_frame *eframe = ctx->saved_psp;
	eframe->r0 = uc->uc_mcontext.arm_r0;
	eframe->r1 = uc->uc_mcontext.arm_r1;
	eframe->r2 = uc->uc_mcontext.arm_r2;
	eframe->r3 = uc->uc_mcontext.arm_r3;
	eframe->r12 = uc->uc_mcontext.arm_ip;
	eframe->lr = uc->uc_mcontext.arm_lr;
	eframe->ra = uc->uc_mcontext.arm_pc;
	eframe->xpsr = uc->uc_mcontext.arm_cpsr;

	/* update signal mask in case uc_sigmask was changed */
	*ss = (k_sigset_t){
		.__bits[0] = uc->uc_sigmask.__bits[0],
		.__bits[1] = uc->uc_sigmask.__bits[1],
	};

	return context_restore(ctx, NULL);
}

void
context_terminate(struct thread *th)
{
	/* nothing to do for now */
}

void
context_free(struct context *ctx)
{
	/* nothing to do for now */
}
