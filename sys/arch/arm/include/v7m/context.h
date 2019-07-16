#ifndef arm_v7m_context_h
#define arm_v7m_context_h

#include <conf/config.h>
#include <stdint.h>

/*
 * Register Synonym Special Role in the procedure call standard
 * ======== ======= ===========================================
 * r15	    PC	    The Program Counter.
 * r14	    LR	    The Link Register.
 * r13	    SP	    The Stack Pointer.
 * r12	    IP	    The Intra-Procedure-call scratch register.
 * r11	    v8, fp  Variable-register 8. Frame pointer.
 * r10	    v7	    Variable-register 7.
 * r9		    Platform register. The meaning of this register is defined
 *		    by the platform standard.
 *	    v6	    Variable-register 6.
 *	    SB	    Static Base (PIC).
 *	    TR	    Thread Register.
 * r8	    v5	    Variable-register 5.
 * r7	    v4	    Variable register 4.
 * r6	    v3	    Variable register 3.
 * r5	    v2	    Variable register 2.
 * r4	    v1	    Variable register 1.
 * r3	    a4	    Argument / scratch register 4.
 * r2	    a3	    Argument / scratch register 3.
 * r1	    a2	    Argument / result / scratch register 2.
 * r0	    a1	    Argument / result / scratch register 1.
 */

/*
 * This frame is automatically pushed/popped to/from the currently active stack
 * by the core on exception entry/exit.
 *
 * Lazy FPU state preservation means that the 17 volatile FPU registers are not
 * preserved unless code within the exception handler uses the FPU.
 *
 * The kernel will preserve non-volatile FPU registers during context switch.
 *
 * ReturnAddress depends on exception type:
 * Type			    Instruction
 * ====			    ===========
 * NMI			    Next
 * SVCall		    Next
 * PendSV		    Next
 * SysTick		    Next
 * External		    Next
 * MemManage		    This
 * UsageFault		    This
 *
 * HardFault, BusFault, DebugMonitor
 *	Synchronous	    This
 *	Asynchronous	    Next
 */
struct exception_frame_basic {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t ra;	    /* ReturnAddress(ExceptionType); */
	uint32_t xpsr;	    /* XPSR<31:10>:frameptralign:XPSR<8:0> */
};

struct exception_frame_extended {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t ra;	    /* ReturnAddress(ExceptionType); */
	uint32_t xpsr;	    /* XPSR<31:10>:frameptralign:XPSR<8:0> */
	uint32_t s0;
	uint32_t s1;
	uint32_t s2;
	uint32_t s3;
	uint32_t s4;
	uint32_t s5;
	uint32_t s6;
	uint32_t s7;
	uint32_t s8;
	uint32_t s9;
	uint32_t s10;
	uint32_t s11;
	uint32_t s12;
	uint32_t s13;
	uint32_t s14;
	uint32_t s15;
	uint32_t fpscr;
	uint32_t pad;	    /* stack must be 8-byte aligned */
};

/*
 * Non-volatile registers switched by context switch.
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

/*
 * System call arguments pushed by system call entry.
 */
struct syscall_args {
	uint32_t a4;			    /* syscall argument 4 */
	uint32_t a5;			    /* syscall argument 5 */
	uint32_t a6;			    /* syscall argument 6 */
	uint32_t syscall;		    /* syscall number */
};

/*
 * Context stored in struct thread
 */
struct context {
	void *tls;		    /* userspace tls address */
	void *ustack;		    /* user stack pointer */
	void *estack;		    /* user->kernel entry stack pointer */
	void *kstack;		    /* kernel stack pointer */
	void *vfork_eframe;	    /* vfork saved exception frame */
};

#endif /* !arm_v7m_context_h */
