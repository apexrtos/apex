/*
 * locore.S - low level platform support
 */

#include <booke/asm_def.h>
#include <conf/config.h>
#include <errno.h>
#include <syscall_table.h>

#warning clear reservations

.text

/*
 * kernel_start
 *
 * Assumptions:
 *   CPU is in privileged state
 *   Machine state (MSR & HID registers) are set
 *   Interrupts are disabled and all interrupt sources are disabled
 *   Clocks and caches are configured
 *   Stack pointer is not set
 *   Enough MMU setup to run kernel
 *   Running in virtual address space
 *   r3 = archive_addr
 *   r4 = archive_size
 *   r5 = machdep0
 *   r6 = machdep1
 */
.global kernel_start
kernel_start:
	/* setup exceptions */
	lis 0, exception_base@h
	mtspr SPRN_IVPR, 0
	li 0, entry_Critical_Input@l
	mtspr SPRN_IVOR0, 0
	li 0, entry_Machine_Check@l
	mtspr SPRN_IVOR1, 0
	li 0, entry_Data_Storage@l
	mtspr SPRN_IVOR2, 0
	li 0, entry_Instruction_Storage@l
	mtspr SPRN_IVOR3, 0
	li 0, entry_External_Input@l
	mtspr SPRN_IVOR4, 0
	li 0, entry_Alignment@l
	mtspr SPRN_IVOR5, 0
	li 0, entry_Program@l
	mtspr SPRN_IVOR6, 0
#if defined(POWER_CAT_FP)
	li 0, entry_Floating_Point_Unavailable@l
	mtspr SPRN_IVOR7, 0
#endif
	li 0, entry_System_Call@l
	mtspr SPRN_IVOR8, 0
#if defined(POWER_IVOR9)
	li 0, entry_Auxiliary_Processor_Unavailable@l
	mtspr SPRN_IVOR9, 0
#endif
	li 0, entry_Decrementer@l
	mtspr SPRN_IVOR10, 0
	li 0, entry_Fixed_Interval_Timer@l
	mtspr SPRN_IVOR11, 0
	li 0, entry_Watchdog_Timer@l
	mtspr SPRN_IVOR12, 0
	li 0, entry_Data_TLB_Error@l
	mtspr SPRN_IVOR13, 0
	li 0, entry_Instruction_TLB_Error@l
	mtspr SPRN_IVOR14, 0
	li 0, entry_Debug@l
	mtspr SPRN_IVOR15, 0
#if defined(POWER_CAT_SP)
	li 0, entry_SPE_EFP_Vector_Unavailable@l
	mtspr SPRN_IVOR32, 0
	li 0, entry_EFP_Data@l
	mtspr SPRN_IVOR33, 0
	li 0, entry_EFP_Round@l
	mtspr SPRN_IVOR34, 0
#endif

	/* setup stack */
	lis 1, __stack@h
	ori 1, 1, (__stack - MIN_FRAME_SIZE)@l

	/* set kernel small data area pointer */
	lis 13, _SDA_BASE_@h
	ori 13, 13, _SDA_BASE_@l

	/* REVISIT: kernel r2? */
	li 2, 0

#if defined(POWER_CAT_SP)
	li 0, KERNEL_SPEFSCR
	mtspr SPRN_SPEFSCR, 0
#endif

	/* start kernel */
	b kernel_main

#if defined(POWER_CAT_SP)
#define FULL_CONTEXT_SAVE_SPECIALS	li 24, 0;   /* spevalid */ \
					mfspr 25, SPRN_SPEFSCR
#define FULL_CONTEXT_STORE_SPECIALS	stmw 24, CONTEXT_FRAME_SPEVALID(1)
#define FULL_CONTEXT_LOAD_SPECIALS	bl spe_load; \
					lmw 25, CONTEXT_FRAME_SPEFSCR(1)
#define FULL_CONTEXT_RESTORE_SPECIALS	mtspr SPRN_SPEFSCR, 25
#else
#define FULL_CONTEXT_SAVE_SPECIALS
#define FULL_CONTEXT_STORE_SPECIALS	stmw 26, CONTEXT_FRAME_XER(1)
#define FULL_CONTEXT_LOAD_SPECIALS	lmw 26, CONTEXT_FRAME_XER(1)
#define FULL_CONTEXT_RESTORE_SPECIALS
#endif

.align 4
exception_base:

/*
 * Entry for all non-recursive IRQs
 *
 * This is used for all IRQs which are not masked by MSR[EE].
 *
 * At entry this, and all lower level IRQs are automatically masked.
 * This function can be interrupted at any point by a higher level IRQ.
 * It must not share any resources with another IRQ level.
 *
 * If this happens while running kernel code it is very likely we will panic.
 * The most likely outcome for user code is to kill the calling thread.
 *
 * If we were running from user mode we know we didn't interrupt kernel code
 * in which case we can safely call kernel functions and reschedule. If we
 * interrupted kernel code we could be in the middle of a critical section in
 * which case the exception needs to be handled _without_ calling any kernel
 * code, or the kernel needs to panic. This means that even if rescheduling is
 * required we _must not_ reschedule kernel threads from here.
 */
#define NON_RECURSIVE_IRQ(num, name, type, srr, return_instruction) \
.align 4; \
.global entry_##name##; \
entry_##name##: \
	mtspr SPRN_##type##_SCRATCH, 2;		/* save r2 */ \
\
	/* get irq stack from cpu_data */ \
	mfspr 2, SPRN_CPU_DATA; \
	lwz 2, CPU_DATA_##type##_IRQ_STACK(2); \
\
	/* push context_frame */ \
	stwu 1, -CONTEXT_FRAME_SIZE(2);		/* backchain */ \
	mr 1, 2;				/* set stack pointer */ \
	stw 0, CONTEXT_FRAME_R0(1); \
	stw 2, CONTEXT_FRAME_R1(1); \
	mfspr 2, SPRN_##type##_SCRATCH;		/* restore r2 */ \
	stmw 2, CONTEXT_FRAME_R2(1);		/* store r2-r31 */ \
	FULL_CONTEXT_SAVE_SPECIALS; \
	mfxer 26; \
	mfctr 27; \
	mflr 28; \
	mfcr 29; \
	mfspr 30, SPRN_##srr##0; \
	mfspr 31, SPRN_##srr##1; \
	FULL_CONTEXT_STORE_SPECIALS; \
\
	/* set kernel small data area pointer */ \
	lis 13, _SDA_BASE_@h; \
	ori 13, 13, _SDA_BASE_@l; \
\
	/* REVISIT: kernel r2? */ \
	li 2, 0; \
\
#if defined(POWER_CAT_SP) \
	li 0, KERNEL_SPEFSCR \
	mtspr SPRN_SPEFSCR, 0 \
#endif \
\
	/* call irq_handler_full_context(exception, context_frame *) */ \
	li 3, num;				/* exception number */ \
	mr 4, 1;				/* context_frame ptr */ \
	bl irq_handler_full_context; \
\
	/* problem (user) threads may require rescheduling */ \
	lwz 3, CONTEXT_FRAME_MSR(1); \
	andi. 0, 3, MSR_PR; \
	beql resched_from_irq_full_context; \
\
	/* pop context_frame */ \
	FULL_CONTEXT_LOAD_SPECIALS; \
	FULL_CONTEXT_RESTORE_SPECIALS; \
	mtxer 26; \
	mtctr 27; \
	mtlr 28; \
	mtcr 29; \
	mtspr SPRN_##srr##0, 30; \
	mtspr SPRN_##srr##1, 31; \
	lwz 0, CONTEXT_FRAME_R0(1); \
	lmw 2, CONTEXT_FRAME_R2(1);		/* r2-r31 */ \
	lwz 1, CONTEXT_FRAME_R1(1); \
	return_instruction

/*
 * Machine Check Interrupt
 *
 * MSR is set to 0
 * ESR or MCSR set depending on implementation
 *
 * If the machine check extension is not implemented this exception is not
 * necessarily recoverable as it reuses the same state as other exceptions,
 * but can nest under them and irrecoverably destroy machine state.
 */
#if defined(POWER_MACHINE_CHECK_EXTENSION)
NON_RECURSIVE_IRQ(1, Machine_Check, MACHINE_CHECK, MCSRR, rfmci)
#else
NON_RECURSIVE_IRQ(1, Machine_Check, CRITICAL, CSRR, rfci)
#endif

/*
 * Debug Interrupt
 *
 * MSR[ME] unchanged all other MSR bits set to 0
 */
#if defined(POWER_CAT_E_ED)
NON_RECURSIVE_IRQ(15, Debug, DEBUG, DSRR, rfdi)
#else
NON_RECURSIVE_IRQ(15, Debug, CRITICAL, CSRR, rfci)
#endif

/*
 * Watchdog Timer Interrupt
 *
 * MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
NON_RECURSIVE_IRQ(12, Watchdog_Timer, CRITICAL, CSRR, rfci)

/*
 * Critical Input Interrupt
 *
 * MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
NON_RECURSIVE_IRQ(0, Critical_Input, CRITICAL, CSRR, rfci)

/*
 * Entry for recursive IRQs
 *
 * This can only be used for IRQs which are masked by MSR[EE].
 *
 * At entry all other recursive IRQs are masked.
 * This function can be interrupted at any point by a higher level IRQ.
 * It must not share any resources with another IRQ level.
 */
#define FAST_RECURSIVE_IRQ(num, name) \
.align 4; \
.global entry_##name##; \
entry_##name##: \
	mtspr SPRN_BASE_SCRATCH0, 2;		/* save r2 */ \
	li 2, num; \
	b fast_recursive_irq

#define FULL_CONTEXT_RECURSIVE_IRQ(num, name) \
.align 4; \
.global entry_##name##; \
entry_##name##: \
	mtspr SPRN_BASE_SCRATCH0, 2;		/* save r2 */ \
	li 2, num; \
	b full_context_recursive_irq

/*
 * Data Storage Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR and DEAR set
 */
FAST_RECURSIVE_IRQ(2, Data_Storage)

/*
 * Instruction Storage Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR set
 */
FAST_RECURSIVE_IRQ(3, Instruction_Storage)

/*
 * Alignment Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR and DEAR set
 */
FAST_RECURSIVE_IRQ(5, Alignment)

/*
 * Program Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR set
 */
FAST_RECURSIVE_IRQ(6, Program)

/*
 * Floating-Point Unavailable Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
#if defined(POWER_CAT_FP)
FAST_RECURSIVE_IRQ(7, Floating_Point_Unavailable)
#endif

/*
 * Auxiliary Processor Unavailable Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
#if defined(POWER_IVOR9)
FAST_RECURSIVE_IRQ(9, Auxiliary_Processor_Unavailable)
#endif

/*
 * SPE/Embedded Floating-Point/Vector Unavailable Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR set
 */
#if defined(POWER_CAT_SP)
FAST_RECURSIVE_IRQ(32, SPE_EFP_Vector_Unavailable)
#endif

/*
 * Embeeded Floating-Point Data Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR set
 */
#if defined(POWER_CAT_SP)
FULL_CONTEXT_RECURSIVE_IRQ(33, EFP_Data)
#endif

/*
 * Embedded Floating-Point Round Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR set
 */
#if defined(POWER_CAT_SP)
FULL_CONTEXT_RECURSIVE_IRQ(34, EFP_Round)
#endif

/*
 * Decrementer Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
FAST_RECURSIVE_IRQ(10, Decrementer)

/*
 * Fixed-Interval Timer Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
FAST_RECURSIVE_IRQ(11, Fixed_Interval_Timer)

/*
 * Data TLB Error Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 * ESR and DEAR set
 */
FAST_RECURSIVE_IRQ(13, Data_TLB_Error)

/*
 * Instruction TLB Error Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
FAST_RECURSIVE_IRQ(14, Instruction_TLB_Error)

/*
 * External Input Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 */
FAST_RECURSIVE_IRQ(4, External_Input)

/*
 * fast_recursive_irq - common irq entry for fast recursive IRQs
 *
 * SPRN_BASE_SCRATCH0 = r2
 * r2 = exception number
 */
.global fast_recursive_irq
fast_recursive_irq:
	mtspr SPRN_BASE_SCRATCH1, 2		/* save exception number */
	mfcr 2
	mtspr SPRN_BASE_SCRATCH2, 2		/* save cr */

	/* check for nested IRQ */
	mfspr 2, SPRN_IRQ_NESTING
	cmplwi 2, 0
	addi 2, 2, 1
	mtspr SPRN_IRQ_NESTING, 2
	mr 2, 1					/* nested irq stack */
	bne 1f

	/* outermost IRQ, switch to irq stack */
	mfspr 2, SPRN_CPU_DATA
	lwz 2, CPU_DATA_BASE_IRQ_STACK(2)	/* outermost irq stack */

	/* push irq_frame */
1:	stwu 1, -IRQ_FRAME_SIZE(2)		/* backchain */
	mr 1, 2					/* set stack pointer */
	stw 0, IRQ_FRAME_R0(1)
	mfspr 2, SPRN_BASE_SCRATCH0		/* restore r2 */
	stw 2, IRQ_FRAME_R2(1)
	stw 3, IRQ_FRAME_R3(1)
	stw 4, IRQ_FRAME_R4(1)
	stw 5, IRQ_FRAME_R5(1)
	stw 6, IRQ_FRAME_R6(1)
	stw 7, IRQ_FRAME_R7(1)
	stw 8, IRQ_FRAME_R8(1)
	stw 9, IRQ_FRAME_R9(1)
	stw 10, IRQ_FRAME_R10(1)
	stw 11, IRQ_FRAME_R11(1)
	stw 12, IRQ_FRAME_R12(1)
	stw 13, IRQ_FRAME_R13(1)
#if defined(POWER_CAT_SP)
	mfspr 0, SPRN_SPEFSCR
	stw 0, IRQ_FRAME_SPEFSCR(1)
#endif
	mfxer 0
	stw 0, IRQ_FRAME_XER(1)
	mfctr 0
	stw 0, IRQ_FRAME_CTR(1)
	mflr 0
	stw 0, IRQ_FRAME_LR(1)
	mfspr 0, SPRN_BASE_SCRATCH2		/* r0 = cr */
	stw 0, IRQ_FRAME_CR(1)
	mfspr 0, SPRN_SRR0
	stw 0, IRQ_FRAME_NIP(1)
	mfspr 0, SPRN_SRR1
	stw 0, IRQ_FRAME_MSR(1)
	mfspr 0, SPRN_ESR
	stw 0, IRQ_FRAME_ESR(1)
	mfspr 0, SPRN_DEAR
	stw 0, IRQ_FRAME_DEAR(1)

	/* set kernel small data area pointer */
	lis 13, _SDA_BASE_@h
	ori 13, 13, _SDA_BASE_@l

	/* REVISIT: kernel r2? */
	li 2, 0

#if defined(POWER_CAT_SP)
	li 0, KERNEL_SPEFSCR
	mtspr SPRN_SPEFSCR, 0
#endif

	/* call irq_handler_min_context(exception, irq_frame *) */
	mfspr 3, SPRN_BASE_SCRATCH1		/* r3 = exception number */
	mr 4, 1
	bl irq_handler_min_context

	/* restore irq_frame except for r2, cr, nip, msr */
	dcbtt 0, 1				/* access backchain soon */
	lwz 0, IRQ_FRAME_R0(1)
	lwz 3, IRQ_FRAME_R3(1)
	lwz 4, IRQ_FRAME_R4(1)
	lwz 5, IRQ_FRAME_R5(1)
	lwz 6, IRQ_FRAME_R6(1)
	lwz 7, IRQ_FRAME_R7(1)
	lwz 8, IRQ_FRAME_R8(1)
	lwz 9, IRQ_FRAME_R9(1)
	lwz 10, IRQ_FRAME_R10(1)
	lwz 11, IRQ_FRAME_R11(1)
	lwz 12, IRQ_FRAME_R12(1)
	lwz 13, IRQ_FRAME_R13(1)
#if defined(POWER_CAT_SP)
	lwz 2, IRQ_FRAME_SPEFSCR(1)
	mtspr SPRN_SPEFSCR, 2
#endif
	lwz 2, IRQ_FRAME_XER(1)
	mtxer 2
	lwz 2, IRQ_FRAME_CTR(1)
	mtctr 2
	lwz 2, IRQ_FRAME_LR(1)
	mtlr 2

	/* disable interrupts while manipulating shared state */
	wrteei 0

	/* decrement IRQ_NESTING */
	mfspr 2, SPRN_IRQ_NESTING
	subi 2, 2, 1
	mtspr SPRN_IRQ_NESTING, 2
	cmplwi 2, 0
	bne 1f					/* nested irq? no resched */

	/* check resched */
	lis 2, resched@h
	ori 2, 2, resched@l
	lwz 2, 0(2)
	cmplwi 2, 0
	bne resched_from_irq			/* resched != 0? */

	/* restore final state and return */
1:	lwz 2, IRQ_FRAME_CR(1)
	mtcr 2
	lwz 2, IRQ_FRAME_NIP(1)
	mtspr SPRN_SRR0, 2
	lwz 2, IRQ_FRAME_MSR(1)
	mtspr SPRN_SRR1, 2
	lwz 2, IRQ_FRAME_R2(1)
	lwz 1, 0(1)				/* pop irq_frame */
	rfi

/*
 * full_context_recursive_irq - common irq entry for full context recursive IRQs
 *
 * SPRN_BASE_SCRATCH0 = r2
 * r2 = exception number
 */
.global full_context_recursive_irq
full_context_recursive_irq:
	mtspr SPRN_BASE_SCRATCH1, 2		/* save exception number */
	mfcr 2
	mtspr SPRN_BASE_SCRATCH2, 2		/* save cr */

	/* check for nested IRQ */
	mfspr 2, SPRN_IRQ_NESTING
	cmplwi 2, 0
	addi 2, 2, 1
	mtspr SPRN_IRQ_NESTING, 2
	mr 2, 1					/* nested irq stack */
	bne 1f

	/* outermost IRQ, switch to irq stack */
	mfspr 2, SPRN_CPU_DATA
	lwz 2, CPU_DATA_BASE_IRQ_STACK(2)	/* outermost irq stack */

	/* push context_frame */
1:	stwu 1, -CONTEXT_FRAME_SIZE(2)		/* backchain */
	stw 0, CONTEXT_FRAME_R0(1)
	stw 2, CONTEXT_FRAME_R1(1)
	mfspr 2, SPRN_BASE_SCRATCH0		/* restore r2 */
	stmw 2, CONTEXT_FRAME_R2(1)		/* store r2-r31 */
	mfspr 3, SPRN_BASE_SCRATCH1		/* r3 = exception number */
	FULL_CONTEXT_SAVE_SPECIALS
	mfxer 26
	mfctr 27
	mflr 28
	mfspr 29, SPRN_BASE_SCRATCH2		/* cr */
	mfspr 30, SPRN_SRR0
	mfspr 31, SPRN_SRR1
	FULL_CONTEXT_STORE_SPECIALS

	/* set kernel small data area pointer */
	lis 13, _SDA_BASE_@h
	ori 13, 13, _SDA_BASE_@l

	/* REVISIT: kernel r2? */
	li 2, 0

#if defined(POWER_CAT_SP)
	li 0, KERNEL_SPEFSCR
	mtspr SPRN_SPEFSCR, 0
#endif

	/* call irq_handler_full_context(exception, context_frame *) */
	mr 4, 1					/* context_frame ptr */
	bl irq_handler_full_context

	/* disable interrupts while manipulating shared state */
	wrteei 0

	/* decrement IRQ_NESTING */
	mfspr 0, SPRN_IRQ_NESTING
	subi 0, 0, 1
	mtspr SPRN_IRQ_NESTING, 0
	cmplwi 0, 0
	bnel resched_from_irq_full_context	/* outermost irq? resched */

	/* pop context_frame */
	FULL_CONTEXT_LOAD_SPECIALS
	FULL_CONTEXT_RESTORE_SPECIALS
	mtxer 26
	mtctr 27
	mtlr 28
	mtcr 29
	mtspr SPRN_SRR0, 30
	mtspr SPRN_SRR1, 31
	lwz 0, CONTEXT_FRAME_R0(1)
	lmw 2, CONTEXT_FRAME_R2(1)		/* r2-r31 */
	lwz 1, CONTEXT_FRAME_R1(1)
	rfi

/*
 * System Call Interrupt
 *
 * MSR[CE], MSR[ME], MSR[DE] unchanged all other MSR bits set to 0
 *
 * Syscall or rescheduling request.
 * Can enter from user mode or kernel mode.
 * If user mode resched might be set and we enter on user stack.
 * If kernel mode resched must be set and we enter on kernel stack.
 *
 * For syscalls:
 * r0 = syscall number
 * r3-r8 = syscall arguments
 */
entry_System_Call:
	mtspr SPRN_BASE_SCRATCH0, 9		/* save r9 */
	mfcr 9
	mtspr SPRN_BASE_SCRATCH1, 9		/* save cr */

	/* check problem state */
	mfspr 9, SPRN_SRR1
	andi. 9, 9, MSR_PR
	mr 9, 1
	beq .Lon_kstack

	/* switch to kernel stack if problem (user) mode */
	lis 9, active_thread@h
	ori 9, 9, active_thread@l
	lwz 9, THREAD_KSTACK(9)
	addi 9, 9, CONFIG_KSTACK_SIZE

.Lon_kstack:
	stwu 1, -CONTEXT_FRAME_SIZE(9)		/* backchain */
	stw 1, CONTEXT_FRAME_R1(9)
	mr 1, 9					/* set stack pointer */
	mflr 9
	stw 9, CONTEXT_FRAME_LR(1)
	mfspr 9, SPRN_BASE_SCRATCH1		/* restore cr */
	stw 9, CONTEXT_FRAME_CR(1)
	mfspr 9, SPRN_BASE_SCRATCH0		/* restore r9 */
	stw 0, CONTEXT_FRAME_R0(1)
	mfspr 0, SPRN_SRR0
	stw 2, CONTEXT_FRAME_R2(1)
	mfspr 2, SPRN_SRR1

	/* all shared state has been stored, safe to allow interrupts */
	wrteei 1

	/* finish writing CONTEXT_FRAME */
	stmw 3, CONTEXT_FRAME_R3(1)		/* store r3-r31 */
	mfxer 9
	stw 9, CONTEXT_FRAME_XER(1)
	mfctr 9
	stw 9, CONTEXT_FRAME_CTR(1)
	stw 0, CONTEXT_FRAME_NIP(1)
	stw 2, CONTEXT_FRAME_MSR(1)

	/* set kernel small data area pointer */
	lis 13, _SDA_BASE_@h
	ori 13, 13, _SDA_BASE_@l

	/* REVISIT: kernel r2? */
	li 2, 0

#if defined(POWER_CAT_SP)
	li 0, KERNEL_SPEFSCR
	mtspr SPRN_SPEFSCR, 0
#endif

	/* check resched */
	lis 9, resched@h
	ori 9, 9, resched@l
	lwz 9, 0(9)
	cmplwi 9, 0
	bne do_resched				/* resched == 0? */

	/* run system call */
	cmplwi 0, SYSCALL_TABLE_SIZE
	// bhi setup_arch_syscall
	/* load syscall... */
#warning todo

.global run_syscall
.run_syscall:
	bctrl
	/* fall through to return_to_user */

.global return_to_user
return_to_user:
	bl sig_deliver
#warning todo

.global setup_arch_syscall
.setup_arch_syscall:
	/* index outside table bounds or null handler */
	lis 9, arch_syscall@h
	ori 9, 9, arch_syscall@l
	mtctr 9
	mr 9, 0					/* arch_syscall sc argument */
	b .run_syscall

/*
 * resched_from_irq_full_context
 *
 * Called from non-recursive IRQs when returning to a problem (user) thread and
 * rescheduling may be necessary.
 *
 * r1 points to a context_frame on an IRQ stack.
 * r2, r13, fscr set for kernel.
 * Interrupts are disabled.
 * Returning to problem (user) mode.
 */
.global resched_from_irq_full_context
resched_from_irq_full_context:
	/* check resched */
	lis 2, resched@h
	ori 2, 2, resched@l
	lwz 2, 0(2)
	cmplwi 2, 0
	beqlr					/* resched == 0? */

	/* get kernel stack for thread */
	lis 2, active_thread@h
	ori 2, 2, active_thread@l		/* r2 = &active_thread */
	lwz 2, 0(2)				/* r2 = active_thread */
	lwz 2, THREAD_KSTACK(2)			/* r2 = kstack */
	addi 2, 2, CONFIG_KSTACK_SIZE - CONTEXT_FRAME_SIZE

#if defined(POWER_CAT_SP)
	mr 3, 1
	bl spe_save_from_irq			/* clobber r0, cr, r16-r31 */
#endif

	/* copy context_frame to kernel stack */
	lmw 3, 0(1)				/* copy 29 words */
	stmw 3, 0(2)
#if defined(POWER_CAT_SP)
#if CONTEXT_FRAME_SIZE != 76 * 4
#error incorrect CONTEXT_FRAME_SIZE
#endif
	cmplwi 5, 0
	lmw 19, (29 * 4)(1)			/* copy 13 words */
	stw 19, (29 * 4)(2)
	beq 1f					/* spevalid != 0? */
	lmw 15, ((29 + 13) * 4)(1)		/* copy 17 words */
	stw 15, ((29 + 13) * 4)(2)
	lmw 15, ((29 + 13 + 17) * 4)(1)		/* copy 17 words */
	stw 15, ((29 + 13 + 17) * 4)(2)
#else
#if CONTEXT_FRAME_SIZE != 40 * 4
#error incorrect CONTEXT_FRAME_SIZE
#endif
	lmw 21, (29 * 4)(1)			/* copy 11 words */
	stw 21, (29 * 4)(1)
#endif

	/* switch to kernel stack */
1:	mr 1, 2

	/* set kernel small data area pointer */
	lis 13, _SDA_BASE_@h
	ori 13, 13, _SDA_BASE_@l

	/* REVISIT: kernel r2? */
	li 2, 0

	b do_resched

/*
 * resched_from_irq
 *
 * Called from recursive IRQs when returning from the outermost interrupt and
 * rescheduling is necessary.
 *
 * r1 points to an irq_frame on the base IRQ stack.
 * r2, r13 _not_ set for kernel. fscr set for kernel.
 * r2, cr trash, all other registers except stack (r1) as per interrupt entry.
 * Interrupts are disabled.
 * Returning to problem (user) or supervisor (kernel) mode.
 */
.global resched_from_irq
resched_from_irq:
	/* get kernel stack for thread */
	lis 2, active_thread@h
	ori 2, 2, active_thread@l		/* r2 = &active_thread */
	lwz 2, 0(2)				/* r2 = active_thread */
	lwz 2, THREAD_KSTACK(2)			/* r2 = kstack */
	addi 2, 2, CONFIG_KSTACK_SIZE - CONTEXT_FRAME_SIZE

	/* write context_frame to kernel stack */
	stmw 3, CONTEXT_FRAME_R3(2)		/* store r3-r31 */
#if defined(POWER_CAT_SP)
	lwz 19, 0(1)				/* backchain */
	li 20, 0				/* link */
	li 21, 0				/* spevalid */
	mfspr 22, SPRN_SPEFSCR
#else
	lwz 21, 0(1)				/* backchain */
	li 22, 0				/* link */
#endif
	mfxer 23
	mfctr 24
	mflr 25
	lwz 26, IRQ_FRAME_CR(1)
	lwz 27, IRQ_FRAME_NIP(1)
	lwz 28, IRQ_FRAME_MSR(1)
	mr 29, 0				/* r0 */
	mr 30, 21				/* r1 */
	lwz 31, IRQ_FRAME_R2(1)			/* r2 */
#if defined(POWER_CAT_SP)
	stmw 19, 0(2)				/* write backchain -> r2 */
	mr 3, 1
	bl spe_save_from_irq			/* clobber r0, cr, r16-r31 */
#else
	stmw 21, 0(2)				/* write backchain -> r2 */
#endif

	/* switch to kernel stack */
	mr 1, 2

	/* set kernel small data area pointer */
	lis 13, _SDA_BASE_@h
	ori 13, 13, _SDA_BASE_@l

	/* REVISIT: kernel r2? */
	li 2, 0

	b do_resched

/*
 * resched_from_thread
 *
 * Called from arch_schedule if we are running from a thread context and
 * rescheduling is necessary.
 *
 * r2, r13 set for kernel.
 * Interrupts are disabled.
 */
.global resched_from_thread
resched_from_thread:
	/* called from C context where volatile registers are preserved
	   by callee so we only need to store non-volatile registers */
	mr 0, 1
	stwu 1, -CONTEXT_FRAME_SIZE(1)		/* backchain */
	/* r0 volatile */
	stw 0, CONTEXT_FRAME_R1(1)
	stw 2, CONTEXT_FRAME_R2(1)
	/* r3-r12 volatile */
	stmw 13, CONTEXT_FRAME_R13(1)		/* store r13-r31 */
	mfcr 29
	mflr 30					/* lr is NIP! */
	mfmsr 31
	stmw 29, CONTEXT_FRAME_CR(1)		/* cr, nip, msr */

#if defined(POWER_CAT_SP)
	/* no need to save context if SPE is not enabled */
	mfmsr 0
	andis. 0, 0, MSR_SPV@h
	li 30, 0				/* spevalid = 0 */
	beq 1f					/* MSR[SPV] == 0 */

	/* r14-r31 nonvolatile */
	evmergehi 14,14,14
	evmergehi 15,15,15
	evmergehi 16,16,16
	evmergehi 17,17,17
	evmergehi 18,18,18
	evmergehi 19,19,19
	evmergehi 20,20,20
	evmergehi 21,21,21
	evmergehi 22,22,22
	evmergehi 23,23,23
	evmergehi 24,24,24
	evmergehi 25,25,25
	evmergehi 26,26,26
	evmergehi 27,27,27
	evmergehi 28,28,28
	evmergehi 29,29,29
	evmergehi 30,30,30
	evmergehi 31,31,31
	stmw 14, CONTEXT_FRAME_RH14(1)		/* save r14-r13 high part */
	li 30, 1				/* spevalid = 1 */

1:	mfspr 31, SPRN_SPEFSCR
	stmw 30, CONTEXT_FRAME_SPEVALID(1)
#endif

	b do_resched

/*
 * do_resched
 *
 * Finally reschedule!
 *
 * r1 points to context_frame on kernel stack for thread.
 * r2, r13 set for kernel.
 * Interrupts are disabled.
 */
.global do_resched
do_resched:
	/* reschedule */
	lis 14, active_thread@h
	ori 14, 14, active_thread@l		/* r14 = &active_thread */
	lwz 3, 0(14)				/* r3 = active_thread */
	stw 1, THREAD_CTX_KSP(3)		/* save stack pointer */
	bl sch_switch				/* switch threads! */
	lwz 3, 0(14)				/* r3 = active_thread */
	lwz 1, THREAD_CTX_KSP(3)		/* load stack pointer */

	/* deliver signals to problem (user) threads */
	lwz 0, CONTEXT_FRAME_MSR(1)
	andi. 0, 0, MSR_PR
	li 3, -EINTERRUPT_RETURN
	bnel sig_deliver

	/* pop context_frame */
#if defined(POWER_CAT_SP)
	bl spe_load
#endif
	lmw 26, CONTEXT_FRAME_XER(1)
	mtxer 26
	mtctr 27
	mtlr 28
	mtcr 29
	mtspr SPRN_SRR0, 30
	mtspr SPRN_SRR1, 31
	lwz 0, CONTEXT_FRAME_R0(1)
	lmw 2, CONTEXT_FRAME_R2(1)		/* r2-r31 */
	lwz 1, CONTEXT_FRAME_R1(1)
	rfi

/*
 * spe_save_from_irq
 *
 * Save SPE context to context_frame if necessary.
 *
 * r3 = context_frame pointer.
 * Interrupts are disabled.
 *
 * Clobbers r0, cr, r16-r31.
 */
#if defined(POWER_CAT_SP)
.global spe_save_from_irq
spe_save_from_irq:
	/* if SPE is enabled context has already been saved */
	mfmsr 0
	andis. 16, 0, MSR_SPV@h
	bnelr					/* MSR[SPV] != 0 */

	/* if context was not using SPE there's nothing to save */
	lwz 16, CONTEXT_FRAME_MSR(3)
	andis. 16, 16, MSR_SPV@h
	bnelr					/* MSR[SPV] != 0 */

	/* return if context has already been saved */
	lwz 16, CONTEXT_FRAME_SPEVALID(3)
	cmplwi 16, 0
	bnelr					/* spevalid != 0 */

	/* temporarily enable SPE for register access */
	oris 16, 0, MSR_SPV@h
	mtmsr 16
	isync

	/* store SPE context */
	li 16, 1
	stw 16, CONTEXT_FRAME_SPEVALID(3)	/* set spevalid = 1 */
	evmergehi 16,16,0
	evmergehi 17,17,1
	evmergehi 18,18,2
	evmergehi 19,19,3
	evmergehi 20,20,4
	evmergehi 21,21,5
	evmergehi 22,22,6
	evmergehi 23,23,7
	evmergehi 24,24,8
	evmergehi 25,25,9
	evmergehi 26,26,10
	evmergehi 27,27,11
	evmergehi 28,28,12
	evmergehi 29,29,13
	evmergehi 30,30,14
	evmergehi 31,31,15
	stmw 16, CONTEXT_FRAME_RH0(3)		/* save r0-r15 high part */
	evmergehi 16,16,16
	evmergehi 17,17,17
	evmergehi 18,18,18
	evmergehi 19,19,19
	evmergehi 20,20,20
	evmergehi 21,21,21
	evmergehi 22,22,22
	evmergehi 23,23,23
	evmergehi 24,24,24
	evmergehi 25,25,25
	evmergehi 26,26,26
	evmergehi 27,27,27
	evmergehi 28,28,28
	evmergehi 29,29,29
	evmergehi 30,30,30
	evmergehi 31,31,31
	stmw 16, CONTEXT_FRAME_RH16(3)		/* save r16-r13 high part */
	evxor 16, 16, 16			/* clear 64-bit r16 */
	evmwumiaa 16, 16, 16			/* r16, ACC = r16 * r16 + ACC */
	evstdd 16, CONTEXT_FRAME_ACC(3)		/* save ACC */

	/* restore MSR */
	mtmsr 0
	isync

	blr
#endif

/*
 * spe_load
 *
 * Load SPE context from context_frame if necessary.
 *
 * r1 = context_frame pointer.
 * Interrupts are disabled.
 *
 * Clobbers r0, cr, r16-r31.
 */
#if defined(POWER_CAT_SP)
.global spe_load
spe_load:
	/* if SPE context was not saved there's nothing to do */
	lwz 0, CONTEXT_FRAME_SPEVALID(1)
	cmplwi 0, 0
	beqlr					/* spevalid == 0 */

	/* temporarily enable SPE for register access */
	mfmsr 0
	oris 16, 0, MSR_SPV@h
	mtmsr 16
	isync

	/* load SPE context */
	evldd 16, CONTEXT_FRAME_ACC(1)		/* load ACC */
	evmra 16, 16				/* r16, ACC = r16 */
	lmw 16, CONTEXT_FRAME_RH0(1)		/* load r0-r15 high part */
	evmergelo 0, 16, 0
	evmergelo 1, 17, 1
	evmergelo 2, 18, 2
	evmergelo 3, 19, 3
	evmergelo 4, 20, 4
	evmergelo 5, 21, 5
	evmergelo 6, 22, 6
	evmergelo 7, 23, 7
	evmergelo 8, 24, 8
	evmergelo 9, 25, 9
	evmergelo 10, 26, 10
	evmergelo 11, 27, 11
	evmergelo 12, 28, 12
	evmergelo 13, 29, 13
	evmergelo 14, 30, 14
	evmergelo 15, 31, 15
	lmw 16, CONTEXT_FRAME_RH16(1)		/* load r16-r31 high part */
	evmergelo 16, 16, 16
	evmergelo 17, 17, 17
	evmergelo 18, 18, 18
	evmergelo 19, 19, 19
	evmergelo 20, 20, 20
	evmergelo 21, 21, 21
	evmergelo 22, 22, 22
	evmergelo 23, 23, 23
	evmergelo 24, 24, 24
	evmergelo 25, 25, 25
	evmergelo 26, 26, 26
	evmergelo 27, 27, 27
	evmergelo 28, 28, 28
	evmergelo 29, 29, 29
	evmergelo 30, 30, 30
	evmergelo 31, 31, 31

	/* restore MSR */
	mtmsr 0
	isync

	blr
#endif
