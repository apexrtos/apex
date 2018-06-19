#ifndef arm_v7m_context_h
#define arm_v7m_context_h

#ifndef __ASSEMBLY__
#include <stdint.h>

/*
 *
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
 *	    v6	    Variagle-register 6.
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
 * These registers are automatically pushed/popped by the core on exception
 * entry/exit.
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
struct exception_frame {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t ra;	    /* ReturnAddress(ExceptionType); */
	uint32_t xpsr;	    /* XPSR<31:10>:frameptralign:XPSR<8:0> */
#if defined(CONFIG_FPU)
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
	uint32_t dummy;	    /* stack must be 8-byte aligned */
#endif
};

/*
 * This frame is pushed when we process a system call
 */
struct syscall_frame {
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
};

/*
 * This frame is pushed when we process an extended system call
 */
struct extended_syscall_frame {
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	struct syscall_frame sframe;
};

/*
 * Registers manually switched in context_switch.
 */
struct nvregs {
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t r11;
	uint32_t lr;
#if defined(CONFIG_FPU)
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
#endif
};

/*
 * This frame is pushed when we create a new userspace thread
 */
struct uthread_frame {
	struct nvregs nvregs;
	struct exception_frame eframe;
	struct extended_syscall_frame esframe;
};

/*
 * This frame is pushed when we create a new kernel thread
 */
struct kthread_frame {
	struct nvregs nvregs;
	struct exception_frame eframe;
};

/*
 * Registers required for switching threads
 */
struct kern_regs {
	uint32_t msp;
	uint32_t psp;
	uint32_t shcsr;
};

struct context {
	void		       *tls;		/* userspace tls address */
	struct kern_regs	kregs;		/* kernel mode registers */
	void		       *saved_psp;	/* psp before signal */
};

#endif /* !__ASSEMBLY__ */

#endif /* !arm_v7m_context_h */
