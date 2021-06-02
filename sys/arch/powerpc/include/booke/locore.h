#pragma once

/*
 * locore.h - Apex low level platform support
 */

#include <cpu.h>
#include <cstddef>

/*
 * cpu_data
 */
struct cpu_data {
	void *base_irq_stack;
	void *critical_irq_stack;
#if defined(POWER_MACHINE_CHECK_EXTENSION)
	void *machine_check_irq_stack;
#endif
#if defined(POWER_CAT_E_ED)
	void *debug_irq_stack;
#endif
};

/*
 * CPU_DATA
 */
union CPU_DATA {
	static constexpr auto SPRN = SPRG0::SPRN;
	const cpu_data *r;
};

/*
 * IRQ_NESTING
 */
union IRQ_NESTING {
	static constexpr auto SPRN = SPRG1::SPRN;
	uint32_t r;
};

/*
 * BASE_SCRATCH0
 */
union BASE_SCRATCH0 {
	static constexpr auto SPRN = SPRG2::SPRN;
};

/*
 * BASE_SCRATCH1
 */
union BASE_SCRATCH1 {
	static constexpr auto SPRN = SPRG3::SPRN;
};

/*
 * BASE_SCRATCH2
 */
union BASE_SCRATCH2 {
	static constexpr auto SPRN = SPRG4::SPRN;
};

/*
 * CRITICAL_SCRATCH
 */
union CRITICAL_SCRATCH {
	static constexpr auto SPRN = SPRG5::SPRN;
};

/*
 * MACHINE_CHECK_SCRATCH
 */
union MACHINE_CHECK_SCRATCH {
	static constexpr auto SPRN = SPRG6::SPRN;
};

/*
 * DEBUG_SCRATCH
 */
#if defined(POWER_CAT_E_ED)
union DEBUG_SCRATCH {
	static constexpr auto SPRN = SPRG9::SPRN;
};
#endif

/*
 * min_frame - minimum stack frame
 */
struct min_frame {
	uint32_t backchain, link;
} __attribute__((aligned(16)));
static_assert(sizeof(min_frame) == 16);
static_assert(offsetof(min_frame, backchain) == 0);

/*
 * irq_frame - minimum context for interrupt handling
 *
 * Volatile registers only, used for most recursive interrupts.
 */
struct irq_frame {
	uint32_t backchain, link;
	uint32_t r0, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13;
#if defined(POWER_CAT_SP)
	uint32_t spefscr;
#endif
	uint32_t xer, ctr, lr, cr, nip, msr, esr, dear;
} __attribute__((aligned(16)));
static_assert(sizeof(irq_frame) % 16 == 0);
static_assert(offsetof(irq_frame, backchain) == 0);

/*
 * context_frame - full CPU context
 *
 * Used for non-recursive interrupts, syscalls, context switching, etc.
 */
struct context_frame {
	uint32_t backchain, link;
#if defined(POWER_CAT_SP)
	uint32_t spevalid;
	uint32_t spefscr;
#endif
	uint32_t xer, ctr, lr, cr, nip, msr;
	uint32_t r[32];			    /* r0, r1, ..., r30, r31 */
#if defined(POWER_CAT_SP)
	uint64_t acc;
	uint32_t rh[32];		    /* high 32-bits of r0 ... r31 */
#endif
} __attribute__((aligned(16)));
static_assert(sizeof(context_frame) % 16 == 0);
static_assert(offsetof(context_frame, backchain) == 0);

#if defined(POWER_CAT_SP)
#warning REVISIT: exceptions?
constexpr auto kernel_spefscr =
	decltype(SPEFSCR::FRMC)(SPEFSCR::RoundingMode::Nearest).r;
#endif

#warning deleteme

#if 0
SPRG0 - PCPU
SPRG1 - STATE?

SPRG2 - SRR_SCRATCH0
SPRG3 - SRR_SCRATCH1
SPRG4 - SRR_SCRATCH2
SPRG5 - CSRR_SCRATCH
SPRG6 - MCSRR_SCRATCH
SPRG9 - DSRR_SCRATCH

#define SRR_SCRATCH0 SPRG1
#define SRR_SCRATCH1 SPRG2

#define CSRR_SCRATCH0 SPRG3
#define CSRR_SCRATCH1 SPRG4

#define MCSRR_SCRATCH0 SPRG5
#define MCSRR_SCRATCH1 SPRG6

#define DSRR_SCRATCH0 SPRG7
#define DSRR_SCRATCH1 SPRG9


#define SPR_IRQ_NESTING SPRG0
#define SPR_SCRATCH SPRG3


if DSRR is implemented we also have SPRG9

/*
 * Apex uses the following SPRs:
 * SPRG0: interrupt nesting counter, schedule flag
 * SPRG1: kernel stack pointer
 * SPRG2: user stack pointer
 * SPRG3: scratch space (user readable)
 */

nesting counter?
active thread?

When we enter an exception NIP and MSR are stored to SRR registers, either
SRR	    SPRG1
CSRR	    SPRG2
MCSRR	    SPRG3
DSRR	    SPRG9

depending on exception & CPU implementation

SRR/CSRR/MCSRR/DSRR can nest, so, we should not share resources between levels
The nesting order shouldn't matter

so we need at least four scratch registers to make this hang together

Treat all exceptions as interrupts?
- Always write state to interrupt stack?
- Would mean copying interrupt state to kernel stack for syscall? inefficient?
- Context switch?

- When to use interrupt stack?


User->Kernel always write state to kernel stack? -- don't forget about signals

IRQ? User->Kernel->IRQ->IRQ?

Actually I think we're going to use the IRQ stack for everything except syscalls.
#endif

/*
 * SPRG0 kernel r/w			irq nesting?
 * SPRG1 kernel r/w			kernel stack for current thread?
 * SPRG2 kernel r/w
 * SPRG3 kernel r/w, maybe user r
 * SPRG4 kernel r/w, user r
 * SPRG5 kernel r/w, user r
 * SPRG6 kernel r/w, user r
 * SPRG7 kernel r/w, user r
 *
 * USPRG0/VRSAVE user r/w
 *
 * SRR0	    syscall NIP save
 * SRR1	    syscall MSR save		restored with rfi
 * CSRR0
 * CSRR1				restored with rfci
 * MCSRR0				optional - machine check extension?
 * MCSRR1				restored with rfmci
 * DSRR0				optional
 * DSRR1				restored with rfdi
 *
 * System V PowerPC ABI
 * r0 Volatile register which may be modified during function linkage
 * r1 Stack frame pointer, always valid
 * r2 System-reserved register
 * r3-r4 Volatile registers used for parameter passing and return values
 * r5-r10 Volatile registers used for parameter passing
 * r11-r12 Volatile registers which may be modified during function linkage
 * r13 Small data area pointer register
 * r14-r30 Registers used for local variables
 * r31 Used for local variables or "environment pointers"
 * f0 Volatile register
 * f1 Volatile register used for parameter passing and return values
 * f2 Volatile registers used for parameter passing
 * f9-f13 Volatile registers
 * f14-f31 Registers used for local variables
 * CR0-CR7 Condition Register Fields, each 4 bits wide
 * LR Link Register
 * CTR Count Register
 * XER Fixed-Point Exception Register
 * FPSCR Floating-Point Status and Control Register
 *
 * r1, r14-r31, r14-f31 are nonvolatile
 * r0, r3-r12, f0-f12, CTR, XER are volatile
 * r0, r11-12 can change between caller and callee (cross module call)
 * r2 is reserved for system use
 * r13 is small data area pointer (set to _SDA_BASE)
 * r31 is "environment pointer" for languages that require it?
 * CR2-CR4 are nonvolatile
 * CR1, CR5-CR7 are volatile
 * FPSCR is volatile
 *
 * r1 - stack pointer, 16 byte alignment, grows down
 * r3-r10, f1-f8 - function parameters
 * r3-r4, f1 - return values
 * CR bit 6 is set when calling vararg function
 *
 * Stack is 16-byte aligned
 * SP points to "back chain word" of lowest allocated frame
 * back chain word always points to previous stack frame
 */

#if 0
/*
 * resched_from_irq - handle rescheduling request from irq
 *
 * This function is called from the outermost interrupt handler if rescheduling
 * is required.

 * NOTES: cleanup
 * how do we synchronise with syscall? could be half way through
   stacking state on kernel stack at this point?
 * because if this is a nested interrupt the PR bit will be 'Problem'.
   Syscalls can _ONLY_ happen from userspace
 * for a kernel thread we need to take care of it with irq nesting?

 *
 * At least EE is disabled, CE, MC and DE may be enabled.
 *
 * r1 = points to irq_frame on SRR, CSRR, MCSRR or DSRR interrupt stack
 * MSR = 0
 * r3 = previous MSR value
 * cr = trash
 * all other registers contain irq entry values
 */
resched_from_irq:
	mtmsr 3					/* restore msr */

#warning WARNING WARNING CANNOT TRUST PR BIT HERE - CHECK STACK ADDRESS INSTEAD
/* if PR is cleared but stack still points to user address we must be in a nested
   interrupt? */

	/* if MSR[PR] is supervisor but stack still points to user stack we
	   have interrupted a lower level IRQ before stack switching */
	mfspr 3, SPRN_##srr##1
	andi. 3, 3, MSR_PR

  50:   7c 08 02 a6     mflr    r0
  54:   90 01 00 04     stw     r0,4(r1)
  58:   94 21 ff f0     stwu    r1,-16(r1)
  5c:   7c 80 00 a6     mfmsr   r4
  60:   2c 83 00 00     cmpwi   cr1,r3,0
  64:   54 84 93 be     rlwinm  r4,r4,18,14,31
  68:   70 84 00 01     andi.   r4,r4,1
  6c:   4e 84 0b 82     cror    4*cr5+lt,4*cr1+lt,gt
  70:   41 94 00 08     blt-    cr5,78 <xxx(unsigned int)+0x28>
  74:   48 00 00 01     bl      74 <xxx(unsigned int)+0x24>
                        74: R_PPC_REL24 interrupted_early()
  78:   48 00 00 01     bl      78 <xxx(unsigned int)+0x28>
                        78: R_PPC_REL24 didnt_interrupt_early()

  50:   7c 80 00 a6     mfmsr   r4
  54:   2c 83 ff ff     cmpwi   cr1,r3,-1
  58:   38 60 00 01     li      r3,1
  5c:   70 84 40 00     andi.   r4,r4,16384
  60:   4e 85 11 c2     crnand  4*cr5+lt,4*cr1+gt,eq
  64:   7c 60 1d 1e     isel    r3,0,r3,20
  68:   4e 80 00 20     blr





	/* increment IRQ nesting? */

	/* user mode interrupts need to switch to kernel stack */
	lwz 3, IRQ_FRAME_MSR(1)
	andi. 3, 3, MSR_PR
	beq .Lswitch_to_kstack

	/* kernel mode irq_frame backchains to kernel stack */
	lwz 3, 0(1)
	stwu 3, -SYSCALL_FRAME_SIZE(3)
	b .Lframe_setup

.Lswitch_to_kstack:
	lis 3, active_thread@h
	ori 3, active_thread@l
	lwz 3, 0(3)
	lwz 3, THREAD_KSTACK(3)
	addi 3, 3, CONFIG_KSTACK_SIZE
	lwz 4, 0(1)
	stwu 4, -SYSCALL_FRAME_SIZE(3)
	lwz 4, IRQ_FRAME_R4(1)

.Lframe_setup:
	/* r1 = irq_frame, r3 = syscall_frame on kernel stack, cr = trash */
	mflr 4
	stw 4, SYSCALL_FRAME_LR(3)
	lwz 4, IRQ_FRAME_CR(1)
	stw 4, SYSCALL_FRAME_CR(3)
	mfxer 4
	stw 4, SYSCALL_FRAME_XER(3)
	mfctr 4
	stw 4, SYSCALL_FRAME_CTR(3)
	lwz 4, IRQ_FRAME_NIP(1)
	stw 4, SYSCALL_FRAME_NIP(3)
	lwz 4, IRQ_FRAME_MSR(1)
	stw 4, SYSCALL_FRAME_MSR(3)
	stw 0, SYSCALL_FRAME_R0(3)
	lwz 4, IRQ_FRAME_R3(1)
	stw 4, SYSCALL_FRAME_R3(3)
	lwz 4, IRQ_FRAME_R4(1)
	stw 4, SYSCALL_FRAME_R4(3)
	stmw 5, SYSCALL_FRAME_R5(3)		/* r5-r31 */

	/* switch to kernel stack, r1 = syscall_frame */
	mr 1, 3

	/* no longer using irq stack, all exceptions can be enabled */
	mfmsr 3
	ori 3, 3, (MSR_EE | MSR_CE | MSR_DE | MSR_ME)
	mtmsr 3

.Ldo_resched:
	bl sch_switch
	li 3, 0					/* sig_deliver argument */
	bl sig_deliver

	/* restore context */
	dcbtt 0, 1
	lwz 3, SYSCALL_FRAME_LR(1)
	mtlr 3
	lwz 3, SYSCALL_FRAME_XER(1)
	mtxer 3
	lwz 3, SYSCALL_FRAME_CTR(1)
	mtctr 3
	lwz 0, SYSCALL_FRAME_R0(1)
	lmw 6, SYSCALL_FRAME_R5(1)		/* r5-r31 */

	/* check if context manipulation is required - this needs to happen
	   with all exceptions disabled as we need to atomically check and
	   return */
	li 3, 0
	mfmsr 4
	mtmsr 3
	mfspr 3, SPRN_SCHED_STATE
	cmp 3, 0				/* loop if zero */
	bne .Lresched_done
	mtmsr 4
	b .Ldo_resched

.Lresched_done
	lwz 3, SYSCALL_FRAME_NIP(1)
	mtspr SPRN_SRR0, 3
	lwz 3, SYSCALL_FRAME_MSR(1)
	mtspr SPRN_SRR1, 3
	lwz 3, SYSCALL_FRAME_CR(1)
	mtcr 3
	lwz 3, SYSCALL_FRAME_R3(1)
	lwz 4, SYSCALL_FRAME_R4(1)
	rfi
	b .					/* stop prefetch */
#endif
