#include <arch/context.h>

#include "exception_frame.h"
#include "mpu.h"
#include <access.h>
#include <alloca.h>
#include <arch/mmio.h>
#include <cpu.h>
#include <cstring>
#include <debug.h>
#include <errno.h>
#include <kmem.h>
#include <sigframe.h>
#include <sys/mman.h>
#include <thread.h>
#include <vm.h>

/* Tell gcc not to use FPU registers */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC target("general-regs-only")
#endif

/*
 * Non-volatile core registers switched by context switch.
 */
struct nvregs {
	uint32_t control;
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	uint32_t lr;
};
static_assert(!(sizeof(nvregs) & 7));

/*
 * Non-volatile FPU registers switched by context switch.
 */
struct fpu_nvregs {
	uint32_t s16;
	uint32_t s17;
	uint32_t s18;
	uint32_t s19;
	uint32_t s20;
	uint32_t s21;
	uint32_t s22;
	uint32_t s23;
	uint32_t s24;
	uint32_t s25;
	uint32_t s26;
	uint32_t s27;
	uint32_t s28;
	uint32_t s29;
	uint32_t s30;
	uint32_t s31;
};
static_assert(!(sizeof(fpu_nvregs) & 7));

/*
 * System call arguments pushed by system call entry.
 */
struct syscall_args {
	uint32_t a4;			    /* syscall argument 4 */
	uint32_t a5;			    /* syscall argument 5 */
	uint32_t a6;			    /* syscall argument 6 */
	uint32_t syscall;		    /* syscall number */
};
static_assert(!(sizeof(syscall_args) & 7));

/*
 * Frame on userspace stack for signal delivery
 */
struct sigframe {
	ucontext_t uc;
	int rval;
	uint32_t pad;
};
static_assert(!(sizeof(sigframe) & 7));

/*
 * Frame on userspace stack for real time signal delivery
 */
struct rt_sigframe {
	sigframe sf;
	siginfo_t si;
};
static_assert(!(sizeof(rt_sigframe) & 7));

/*
 * System call return entry point
 */
extern "C" void syscall_ret();

/*
 * Test link register to determine if exception frame is basic or extended
 */
static bool
is_exception_frame_extended(uint32_t lr)
{
#if defined(CONFIG_FPU)
	return !(lr & EXC_NOT_FPCA);
#else
	return false;
#endif
}

/*
 * arch_schedule - call sch_switch as soon as possible
 *
 * In order to switch threads synchronously with any other interrupt sources
 * we run the thread switch in exc_PendSV.
 */
void
arch_schedule()
{
	write32(&SCB->ICSR, {.PENDSVSET = 1});

	/* make sure the write to ICSR happens before the next instruction */
	asm volatile("dsb");
}

/*
 * context_switch - switch thread contexts
 */
void
context_switch(thread *prev, thread *next)
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
context_init_idle(context *ctx, void *kstack_top)
{
	/* nothing to do */
}

/*
 * Initialise context for kernel thread
 */
void
context_init_kthread(context *ctx, void *v_kstack_top, void (*entry)(void *),
		     void *arg)
{
	char *kstack_top = (char *)v_kstack_top;

	/* stack must be 8-byte aligned */
	assert(!((uint32_t)kstack_top & 7));

	/* stack layout for new kernel thread */
	struct stack {
#if defined(CONFIG_FPU)
		fpu_nvregs fpu;
#endif
		nvregs nv;
		exception_frame_basic ef;
	};

	/* allocate a new kernel thread stack */
	kstack_top -= sizeof(stack);
	stack *s = reinterpret_cast<stack *>(kstack_top);

	/* set thread arguments */
	s->ef.r0 = (uint32_t)arg;

	/* loading an unaligned value from the stack into the PC on an
	   exception return is unpredictable */
	s->ef.ra = (uint32_t)entry & -2;
	s->ef.xpsr = EPSR_T;
	s->nv.control = 0;
	s->nv.lr = EXC_RETURN_THREAD_PROCESS_BASIC;

	/* initialise context */
	ctx->ksp = kstack_top;
}

/*
 * Initialise context for userspace thread
 */
int
context_init_uthread(context *child, as *as, void *v_kstack_top,
		     void *v_ustack_top, void (*entry)(), long rval)
{
	context *parent = &thread_cur()->ctx;
	bool shared_ustack = false;

	/* if thread was created by vfork it shares stack with parent */
	if (!v_ustack_top) {
		shared_ustack = true;
		v_ustack_top = parent->usp;
	}
	assert(v_ustack_top);

	/* stack must be 8-byte aligned */
	assert(!((uint32_t)v_kstack_top & 7));
	assert(!((uint32_t)v_ustack_top & 7));

	char *kstack_top = (char *)v_kstack_top;
	char *ustack_top = (char *)v_ustack_top;

	/* stack layout for new userspace thread */
	struct stack {
#if defined(CONFIG_FPU)
		fpu_nvregs fpu;
#endif
		nvregs knv;		    /* kernel context */
		exception_frame_basic ef;   /* for return to thread */
		syscall_args args;
		nvregs unv;		    /* user context */
	};

	/* allocate a new thread frame */
	void *kstack = kstack_top;
	kstack_top -= sizeof(stack);
	stack *s = (stack *)kstack_top;

	/* threads created by fork/vfork/clone don't specify an entry point and
	   must return to userspace as an exact clone of their parent */
	if (!entry) {
		/* copy user non volatile registers from parent */
		s->unv = *(nvregs *)((char *)parent->kstack - sizeof(nvregs));

		/* copy tls pointer */
		child->tls = parent->tls;

		/* copy or preserve userspace exception frame */
		const ssize_t sz = is_exception_frame_extended(s->unv.lr)
		    ? sizeof(exception_frame_extended)
		    : sizeof(exception_frame_basic);
		if (!shared_ustack) {
			/* copy userspace exception frame */
			ustack_top -= sz;
			if (vm_copy(as, ustack_top, parent->usp, sz) != sz)
				return DERR(-EFAULT);
		} else {
			/* preserve userspace exception frame - this is needed
			   for vfork to work as returning to userspace will
			   destroy current exception frame */
			assert(!parent->vfork_eframe);
			parent->vfork_eframe = kmem_alloc(sz, MA_NORMAL);
			if (!parent->vfork_eframe)
				return DERR(-ENOMEM);
			if (vm_read(as, parent->vfork_eframe, parent->usp,
			    sz) != sz)
				return DERR(-EFAULT);
		}
	} else {
		assert(!shared_ustack);

		/* allocate a new exception frame for return to userspace */
		const size_t sz = sizeof(exception_frame_basic);
		ustack_top -= sz;

		/* Loading an unaligned value from the stack into the PC on an
		   exception return is UNPREDICTABLE. */
		exception_frame_basic ef = {
			.ra = (uint32_t)entry & -2,
			.xpsr = EPSR_T,
		};
		if (vm_write(as, &ef, ustack_top, sz) != sz)
			return DERR(-EFAULT);
		s->unv.lr = EXC_RETURN_THREAD_PROCESS_BASIC;
		s->unv.control = CONTROL_NPRIV;
	}

	/* set syscall return value */
	s->ef.r0 = rval;

	/* Loading an unaligned value from the stack into the PC on an
	   exception return is UNPREDICTABLE. */
	s->ef.ra = (uint32_t)syscall_ret & -2;
	s->ef.xpsr = EPSR_T;
	s->knv.control = 0;
	s->knv.lr = EXC_RETURN_THREAD_PROCESS_BASIC;

	/* initialise context */
	child->usp = ustack_top;
	child->kstack = kstack;
	child->ksp = kstack_top;

	return 0;
}

/*
 * Restore context after vfork
 */
void
context_restore_vfork(context *ctx, as *as)
{
	/* thread was not created by vfork so nothing to restore */
	if (!ctx->vfork_eframe)
		return;

	/* restore userspace exception frame */
	nvregs *unv = (nvregs *)((char *)ctx->kstack - sizeof(nvregs));
	const size_t sz = is_exception_frame_extended(unv->lr)
	    ? sizeof(exception_frame_extended)
	    : sizeof(exception_frame_basic);
	vm_write(as, ctx->vfork_eframe, ctx->usp, sz);
	kmem_free(ctx->vfork_eframe);
	ctx->vfork_eframe = 0;
}

/*
 * Save FPU state to vfp_sigframe struct
 */
static void
fpu_save(vfp_sigframe *f, const exception_frame_extended *uef)
{
#if defined(CONFIG_FPU)
	f->magic = VFP_SIGFRAME_MAGIC;
	f->size = sizeof(*f);
	memcpy(f->regs, &uef->s0, 16 * 4);
	asm("vstmia %1, {s16-s31}"
		: "=m"(*(uint32_t(*)[16])(f->regs + 16))
		: "r"(f->regs + 16));
	f->fpscr = uef->fpscr;
#endif
}

/*
 * Load FPU state from vfp_sigframe struct
 */
static void
fpu_load(const vfp_sigframe *f, exception_frame_extended *uef)
{
#if defined(CONFIG_FPU)
	memcpy(&uef->s0, f->regs, 16 * 4);
	asm("vldmia %1, {s16-s31}"
		: : "m"(*(uint32_t(*)[16])(f->regs + 16)), "r"(f->regs + 16));
	uef->fpscr = f->fpscr;
#endif
}

/*
 * Test if p points to a vfp_sigframe struct
 */
static bool
fpu_present(const void *p)
{
#if defined(CONFIG_FPU)
	const vfp_sigframe *f = (vfp_sigframe *)p;
	return f->magic == VFP_SIGFRAME_MAGIC && f->size == sizeof(*f);
#else
	return false;
#endif
}

/*
 * Synchronise FPU context
 *
 * This will trigger lazy state preservation if it hasn't already happened
 */
static void
fpu_lazy_sync()
{
#if defined(CONFIG_FPU)
	asm("vmov.f32 s0, #1.0");
#endif
}

/*
 * Drop FPU context
 *
 * This will stop the core from performing lazy state preservation
 */
static void
fpu_lazy_drop()
{
#if defined(CONFIG_FPU)
	fpu::fpccr r = read32(&FPU->FPCCR);
	r.LSPACT = 0;
	write32(&FPU->FPCCR, r);
#endif
}

/*
 * Setup context for signal delivery
 *
 * Always called in handler mode on interrupt stack.
 */
bool
context_set_signal(context *ctx, const k_sigset_t *ss,
    void (*handler)(int), void (*restorer)(), const int sig,
    const siginfo_t *si, const int rval)
{
	/* can't signal kernel thread */
	if (!ctx->usp)
		panic("signal kthread");

	/* CCR_STKALIGN guarantees 8 byte stack alignment after exception */
	assert(!((uintptr_t)ctx->usp & 7));

	/* registers stored on entry to kernel */
	nvregs *unv = (nvregs *)((char *)ctx->kstack - sizeof(*unv));

	/* userspace exception frame from kernel entry */
	const bool uef_extended = is_exception_frame_extended(unv->lr);
	const size_t uef_sz = uef_extended
	    ? sizeof(exception_frame_extended)
	    : sizeof(exception_frame_basic);

	/* make sure FPU registers are written to userspace exception frame */
	if (uef_extended)
		fpu_lazy_sync();

	/* allocate stack frame for signal */
	char *usp = (char *)ctx->usp + uef_sz;
	exception_frame_basic *sef;
	sigframe *ssf;
	siginfo_t *ssi = 0;
	if (si) {
		struct rt_sigframe_ef {
			exception_frame_basic ef;
			rt_sigframe rsf;
		} *f = (rt_sigframe_ef *)(usp -= sizeof(*f));
		sef = &f->ef;
		ssf = &f->rsf.sf;
		ssi = &f->rsf.si;
	} else {
		struct sigframe_ef {
			exception_frame_basic ef;
			sigframe sf;
		} *f = (sigframe_ef *)(usp -= sizeof(*f));
		sef = &f->ef;
		ssf = &f->sf;
	}

	/* catch stack overflow */
	if (!u_access_ok(usp, (char *)ctx->usp + uef_sz - usp, PROT_WRITE))
		return DERR(false);

	/* make a copy of the userspace exception frame as we are going to
	 * overwrite it's location with the signal frame */
	exception_frame_extended *uef = (exception_frame_extended *)alloca(uef_sz);
	memcpy(uef, ctx->usp, uef_sz);

	/* did exception entry add 4 bytes to align the userspace stack? */
	const size_t uef_align = uef->xpsr & XPSR_FRAMEPTRALIGN ? 4 : 0;

	/* initialise userspace signal context */
	memset(ssf, 0, sizeof(*ssf));
	ssf->uc.uc_mcontext = (mcontext_t){
		.arm_r0 = uef->r0,
		.arm_r1 = uef->r1,
		.arm_r2 = uef->r2,
		.arm_r3 = uef->r3,
		.arm_r4 = unv->r4,
		.arm_r5 = unv->r5,
		.arm_r6 = unv->r6,
		.arm_r7 = unv->r7,
		.arm_r8 = unv->r8,
		.arm_r9 = unv->r9,
		.arm_r10 = unv->r10,
		.arm_fp = unv->r11,
		.arm_ip = uef->r12,
		.arm_sp = (uintptr_t)ctx->usp + uef_sz + uef_align,
		.arm_lr = uef->lr,
		.arm_pc = uef->ra,
		.arm_cpsr = uef->xpsr,
	};
	ssf->uc.uc_sigmask.__bits[0] = ss->__bits[0];
	ssf->uc.uc_sigmask.__bits[1] = ss->__bits[1];
	if (uef_extended)
		fpu_save((vfp_sigframe *)ssf->uc.uc_regspace, uef);
	if (si)
		*ssi = *si;
	ssf->rval = rval;

	/* build new exception frame for signal delivery */
	sef->r0 = sig;
	sef->r1 = (uint32_t)ssi;
	sef->r2 = (uint32_t)&ssf->uc;
	sef->lr = (uint32_t)restorer;
	sef->ra = (uint32_t)handler & -2;
	sef->xpsr = EPSR_T;

	/* adjust nvregs to match signal exception frame type */
	unv->lr = EXC_RETURN_THREAD_PROCESS_BASIC;

	/* adjust stack pointer for signal delivery */
	ctx->usp = sef;

	return true;
}

/*
 * Set thread local storage pointer in context
 */
void
context_set_tls(context *ctx, void *tls)
{
	ctx->tls = tls;
}

/*
 * Restore signal context
 */
bool
context_restore(context *ctx, k_sigset_t *ss, int *rval, bool siginfo)
{
	if (!ctx->usp)
		panic("signal kthread");

	/* CCR_STKALIGN guarantees 8 byte stack alignment after exception */
	assert(!((uintptr_t)ctx->usp & 7));

	/* registers stored on sigreturn entry to kernel */
	nvregs *unv = (nvregs *)((char *)ctx->kstack - sizeof(*unv));

	/* userspace exception frame from sigreturn kernel entry */
	const bool sef_extended = is_exception_frame_extended(unv->lr);
	const size_t sef_sz = sef_extended
	    ? sizeof(exception_frame_extended)
	    : sizeof(exception_frame_basic);

	/* throw away any FPU context from signal handler */
	if (sef_extended)
		fpu_lazy_drop();

	/* retrieve signal frame from user stack */
	sigframe sf;
	void *ssp = (char *)ctx->usp + sef_sz;

	/* check access to signal frame on user stack */
	if (!u_access_ok(ssp, sizeof(sf), PROT_READ))
		return DERR(false);

	/* make a copy of the signal frame as we are going to overwrite it's
	 * location with the exception frame below */
	memcpy(&sf, ssp, sizeof(sf));

	/* get userspace stack pointer */
	char *usp = (char *)(sf.uc.uc_mcontext.arm_sp & -7);

	/* size of exception frame depends on whether there's FPU context */
	const bool uef_extended = fpu_present(sf.uc.uc_regspace);
	const size_t uef_sz = uef_extended
	    ? sizeof(exception_frame_extended)
	    : sizeof(exception_frame_basic);

	/* allocate exception frame on userspace stack */
	usp -= uef_sz;
	exception_frame_extended *uef = (exception_frame_extended *)usp;

	/* check access to exception frame on userspace stack */
	if (!u_access_ok(uef, uef_sz, PROT_WRITE))
		return DERR(false);

	/* restore state */
	uef->r0 = sf.uc.uc_mcontext.arm_r0;
	uef->r1 = sf.uc.uc_mcontext.arm_r1;
	uef->r2 = sf.uc.uc_mcontext.arm_r2;
	uef->r3 = sf.uc.uc_mcontext.arm_r3;
	unv->r4 = sf.uc.uc_mcontext.arm_r4;
	unv->r5 = sf.uc.uc_mcontext.arm_r5;
	unv->r6 = sf.uc.uc_mcontext.arm_r6;
	unv->r7 = sf.uc.uc_mcontext.arm_r7;
	unv->r8 = sf.uc.uc_mcontext.arm_r8;
	unv->r9 = sf.uc.uc_mcontext.arm_r9;
	unv->r10 = sf.uc.uc_mcontext.arm_r10;
	unv->r11 = sf.uc.uc_mcontext.arm_fp;
	uef->r12 = sf.uc.uc_mcontext.arm_ip;
	uef->lr = sf.uc.uc_mcontext.arm_lr;
	uef->xpsr = sf.uc.uc_mcontext.arm_cpsr;
	uef->ra = sf.uc.uc_mcontext.arm_pc & -2;
	ss->__bits[0] = sf.uc.uc_sigmask.__bits[0];
	ss->__bits[1] = sf.uc.uc_sigmask.__bits[1];
	if (uef_extended)
		fpu_load((vfp_sigframe *)sf.uc.uc_regspace, uef);

	/* adjust nvregs for userspace return */
	unv->control = CONTROL_NPRIV;
	unv->lr = uef_extended
	    ? EXC_RETURN_THREAD_PROCESS_EXTENDED
	    : EXC_RETURN_THREAD_PROCESS_BASIC;

	/* set userspace stack pointer */
	ctx->usp = usp;

	*rval = sf.rval;
	return true;
}

/*
 * context_termiante - thread is terminating
 */
void
context_terminate(thread *th)
{
	/* If thread is terminating due to exec it's userspace stack has been
	   unmapped and is no longer writable. Make sure the core doesn't try
	   to preserve any FPU state to the old stack. */
	fpu_lazy_drop();

	th->ctx.usp = 0;

#if defined(CONFIG_MPU)
	mpu_thread_terminate(th);
#endif
}

void
context_free(context *ctx)
{
	kmem_free(ctx->vfork_eframe);
}
