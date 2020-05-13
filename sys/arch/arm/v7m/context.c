#include <arch.h>

#include "mpu.h"
#include <access.h>
#include <cpu.h>
#include <debug.h>
#include <errno.h>
#include <kmem.h>
#include <sys/mman.h>
#include <thread.h>
#include <vm.h>

extern void syscall_ret(void);

/*
 * arch_schedule - call sch_switch as soon as possible
 *
 * In order to switch threads synchronously with any other interrupt sources
 * we run the thread switch in exc_PendSV.
 */
void
arch_schedule(void)
{
	write32(&SCB->ICSR, (union scb_icsr){.PENDSVSET = 1}.r);

	/* make sure the write to ICSR happens before the next instruction */
	asm volatile("dsb");
}

/*
 * context_switch - switch thread contexts
 */
void
context_switch(struct thread *prev, struct thread *next)
{
	/* context switch handled in PendSV */
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

	/* stack layout for new kernel thread */
	struct stack {
#if defined(CONFIG_FPU)
		struct fpu_nvregs fpu;
#endif
		struct nvregs nv;
		struct exception_frame_basic ef;
	};

	/* allocate a new kernel thread stack */
	kstack_top -= sizeof(struct stack);
	struct stack *s = kstack_top;

	/* set thread arguments */
	s->ef.r0 = (uint32_t)arg;

	/* loading an unaligned value from the stack into the PC on an
	   exception return is unpredictable */
	s->ef.ra = (uint32_t)entry & -2;
	s->ef.xpsr = EPSR_T;
	s->nv.control = 0;
	s->nv.lr = EXC_RETURN_THREAD_PROCESS_BASIC;

	/* initialise context */
	ctx->kstack = kstack_top;
}

/*
 * Initialise context for userspace thread
 */
int
context_init_uthread(struct context *child, struct as *as, void *kstack_top,
    void *ustack_top, void (*entry)(void), long retval)
{
	struct context *parent = &thread_cur()->ctx;
	bool shared_ustack = false;

	/* if thread was created by vfork it shares stack with parent */
	if (!ustack_top) {
		shared_ustack = true;
		ustack_top = parent->ustack;
	}
	assert(ustack_top);

	/* stack must be 8-byte aligned */
	assert(!((uint32_t)kstack_top & 7));
	assert(!((uint32_t)ustack_top & 7));

	/* stack layout for new userspace thread */
	struct stack {
#if defined(CONFIG_FPU)
		struct fpu_nvregs fpu;
#endif
		struct nvregs knv;		    /* kernel context */
		struct exception_frame_basic ef;    /* for return to thread */
		struct syscall_args args;
		struct nvregs unv;		    /* user context */
	};

	/* allocate a new thread frame */
	void *estack_top = kstack_top;
	kstack_top -= sizeof(struct stack);
	struct stack *s = kstack_top;

	/* threads created by fork/vfork/clone don't specify an entry point and
	   must return to userspace as an exact clone of their parent */
	if (!entry) {
		/* copy user non volatile registers from parent */
		s->unv = *(struct nvregs *)(parent->estack -
		    sizeof(struct nvregs));

		/* copy tls pointer */
		child->tls = parent->tls;

		/* copy or preserve userspace exception frame */
		const size_t sz = s->unv.lr & EXC_NOT_FPCA
		    ? sizeof(struct exception_frame_basic)
		    : sizeof(struct exception_frame_extended);
		if (!shared_ustack) {
			/* copy userspace exception frame */
			ustack_top -= sz;
			if (vm_copy(as, ustack_top, parent->ustack, sz) != sz)
				return DERR(-EFAULT);
		} else {
			/* preserve userspace exception frame - this is needed
			   for vfork to work as returning to userspace will
			   destroy current exception frame */
			assert(!parent->vfork_eframe);
			parent->vfork_eframe = kmem_alloc(sz, MA_NORMAL);
			if (!parent->vfork_eframe)
				return DERR(-ENOMEM);
			if (vm_read(as, parent->vfork_eframe, parent->ustack,
			    sz) != sz)
				return DERR(-EFAULT);
		}
	} else {
		assert(!shared_ustack);

		/* allocate a new exception frame for return to userspace */
		const size_t sz = sizeof(struct exception_frame_basic);
		ustack_top -= sz;

		/* Loading an unaligned value from the stack into the PC on an
		   exception return is UNPREDICTABLE. */
		struct exception_frame_basic ef = {
			.ra = (uint32_t)entry & -2,
			.xpsr = EPSR_T,
		};
		if (vm_write(as, &ef, ustack_top, sz) != sz)
			return DERR(-EFAULT);
		s->unv.lr = EXC_RETURN_THREAD_PROCESS_BASIC;
		s->unv.control = CONTROL_NPRIV;
	}

	/* set syscall return value */
	s->ef.r0 = retval;

	/* Loading an unaligned value from the stack into the PC on an
	   exception return is UNPREDICTABLE. */
	s->ef.ra = (uint32_t)syscall_ret & -2;
	s->ef.xpsr = EPSR_T;
	s->knv.control = 0;
	s->knv.lr = EXC_RETURN_THREAD_PROCESS_BASIC;

	/* initialise context */
	child->ustack = ustack_top;
	child->estack = estack_top;
	child->kstack = kstack_top;

	return 0;
}

/*
 * Restore context after vfork
 */
void
context_restore_vfork(struct context *ctx, struct as *as)
{
	/* thread was not created by vfork so nothing to restore */
	if (!ctx->vfork_eframe)
		return;

	/* restore userspace exception frame */
	struct nvregs *unv = ctx->estack - sizeof(struct nvregs);
	const size_t sz = unv->lr & EXC_NOT_FPCA
	    ? sizeof(struct exception_frame_basic)
	    : sizeof(struct exception_frame_extended);
	vm_write(as, ctx->vfork_eframe, ctx->ustack, sz);
	kmem_free(ctx->vfork_eframe);
	ctx->vfork_eframe = 0;
}

/*
 * Return true if context is already setup for signal delivery
 */
static bool
context_in_signal(struct context *ctx)
{
	struct thread *th = thread_cur();

	/* kernel threads can't be in signal handler */
	assert(ctx->ustack);

	/* get kernel stack for thread */
	assert(&th->ctx == ctx);
	void *kstack_top = arch_kstack_align(th->kstack + CONFIG_KSTACK_SIZE);

	/* if there is saved context on kernel stack signal is in progress */
	return ctx->estack != kstack_top;
}

/*
 * Setup context for signal delivery
 *
 * Always called in handler mode on interrupt stack.
 */
bool
context_set_signal(struct context *ctx, const k_sigset_t *ss,
    void (*handler)(int), void (*restorer)(void), const int sig,
    const siginfo_t *si, const int rval)
{
	/* can't signal kernel thread */
	if (!ctx->ustack)
		panic("signal kthread");

	if (context_in_signal(ctx))
		return DERR(false);

	/* allocate user stack */
	void *usp = ctx->ustack;
	const struct exception_frame_basic *uef = usp;
	ucontext_t *uuc = 0;
	siginfo_t *usi = 0;
	struct k_sigset_t *uss = 0;
	if (si) {
		usp -= sizeof(ucontext_t);
		uuc = usp;
		usp -= sizeof(siginfo_t);
		usi = usp;
	} else {
		usp -= sizeof(struct k_sigset_t);
		uss = usp;
	}
	usp -= 8;		    /* stack must be 8-byte aligned */
	int *urval = usp;
	usp -= sizeof(struct exception_frame_basic);
	struct exception_frame_basic *usef = usp;

	/* catch stack overflow */
	if (!u_access_ok(usp, ctx->ustack - usp + sizeof(*uef), PROT_WRITE))
		return DERR(false);

	/* initialise userspace signal context */
	if (si) {
		const struct nvregs *unv = ctx->estack - sizeof(*unv);
		*uuc = (ucontext_t){
			.uc_flags = 0,
			.uc_link = 0,
			.uc_stack = (stack_t){
				.ss_sp = 0,
				.ss_flags = 0,
				.ss_size = 0,
			},
			.uc_mcontext = (mcontext_t){
				.trap_no = 0,
				.error_code = 0,
				.oldmask = 0,
				.arm_r0 = uef->r0,
				.arm_r1 = uef->r1,
				.arm_r2 = uef->r2,
				.arm_r3 = uef->r3,
				.arm_r4 = unv->r4,
				.arm_r4 = unv->r5,
				.arm_r6 = unv->r6,
				.arm_r7 = unv->r7,
				.arm_r8 = unv->r8,
				.arm_r9 = unv->r9,
				.arm_r10 = unv->r10,
				.arm_fp = unv->r11,
				.arm_ip = uef->r12,
				.arm_lr = uef->lr,
				.arm_pc = uef->ra,
				.arm_cpsr = uef->xpsr,
				.fault_address = 0,
			},
			.uc_sigmask = (sigset_t){
				.__bits[0] = ss->__bits[0],
				.__bits[1] = ss->__bits[1],
			},
			.uc_regspace = {},
		};
		*usi = *si;
	} else
		*uss = *ss;
	*urval = rval;

	/* build new user exception frame for signal */
	usef->r0 = sig;
	usef->r1 = (uint32_t)usi;
	usef->r2 = (uint32_t)uuc;
	usef->lr = (uint32_t)restorer;
	usef->ra = (uint32_t)handler & -2;
	usef->xpsr = EPSR_T;

	/* build new nvregs for signal */
	struct nvregs *knv = ctx->estack - 2 * sizeof(struct nvregs);
	knv->control = CONTROL_NPRIV;
	knv->lr = EXC_RETURN_THREAD_PROCESS_BASIC;

	/* adjust stack pointers for signal delivery */
	ctx->ustack = usef;
	ctx->estack -= sizeof(struct nvregs);

	return true;
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
 * Restore signal context
 */
bool
context_restore(struct context *ctx, k_sigset_t *ss, int *rval, const bool info)
{
	if (!ctx->ustack)
		panic("signal kthread");

	if (!context_in_signal(ctx))
		return DERR(false);

	struct nvregs *unv = ctx->estack - sizeof(struct nvregs);

	/* size of exception frame from sigreturn */
#if defined(CONFIG_FPU)
	const size_t sz = unv->lr & EXC_NOT_FPCA
	    ? sizeof(struct exception_frame_basic)
	    : sizeof(struct exception_frame_extended);
#else
	const size_t sz = sizeof(struct exception_frame_basic);
#endif

	/* retrieve data from user stack */
	void *usp = ctx->ustack;
	usp += sz;
	const int *urval = usp;
	usp += 8;
	const ucontext_t *uuc = 0;
	const struct k_sigset_t *uss = 0;
	if (info) {
		usp += sizeof(siginfo_t);
		uuc = usp;
		usp += sizeof(ucontext_t);
	} else {
		uss = usp;
		usp += sizeof(struct k_sigset_t);
	}
	struct exception_frame_basic *uef = usp;
	usp += sizeof(struct exception_frame_basic);

	/* check access to user memory */
	if (!u_access_ok(ctx->ustack, usp - ctx->ustack, PROT_READ))
		return DERR(false);

	/* restore state */
	if (info) {
		uef->r0 = uuc->uc_mcontext.arm_r0;
		uef->r1 = uuc->uc_mcontext.arm_r1;
		uef->r2 = uuc->uc_mcontext.arm_r2;
		uef->r3 = uuc->uc_mcontext.arm_r3;
		unv->r4 = uuc->uc_mcontext.arm_r4;
		unv->r5 = uuc->uc_mcontext.arm_r5;
		unv->r6 = uuc->uc_mcontext.arm_r6;
		unv->r7 = uuc->uc_mcontext.arm_r7;
		unv->r8 = uuc->uc_mcontext.arm_r8;
		unv->r9 = uuc->uc_mcontext.arm_r9;
		unv->r10 = uuc->uc_mcontext.arm_r10;
		unv->r11 = uuc->uc_mcontext.arm_fp;
		uef->r12 = uuc->uc_mcontext.arm_ip;
		uef->lr = uuc->uc_mcontext.arm_lr;
		if (uef->ra != uuc->uc_mcontext.arm_pc) {
			uef->ra = uuc->uc_mcontext.arm_pc;
			/*
			 * If we are returning to a different instruction the
			 * old instruction continuation bits are not valid.
			 */
			uef->xpsr = uuc->uc_mcontext.arm_cpsr & ~EPSR_ICI_IT;
		}
		*ss = (k_sigset_t){
			.__bits[0] = uuc->uc_sigmask.__bits[0],
			.__bits[1] = uuc->uc_sigmask.__bits[1],
		};
	} else
		*ss = *uss;

	/* restore stack pointers */
	ctx->ustack = uef;
	ctx->estack += sizeof(struct nvregs);

	*rval = *urval;
	return true;
}

/*
 * context_termiante - thread is terminating
 */
void
context_terminate(struct thread *th)
{
#if defined(CONFIG_FPU)
	/* If thread is terminating due to exec it's userspace stack has been
	   unmapped and is no longer writable. Make sure the core doesn't try
	   to preserve any FPU state to the old stack. */
	union fpu_fpccr fpccr = read32(&FPU->FPCCR);
	fpccr.LSPACT = 0;
	write32(&FPU->FPCCR, fpccr.r);
#endif
	th->ctx.ustack = 0;
#if defined(CONFIG_MPU)
	mpu_thread_terminate(th);
#endif
}

void
context_free(struct context *ctx)
{
	kmem_free(ctx->vfork_eframe);
}
