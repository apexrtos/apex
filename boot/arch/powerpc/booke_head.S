/*
 * booke_head.S
 *
 * setup machine and jump to crt_init
 */

#include <booke/asm_def.h>

.text

/*
 * _start
 *
 * Boot loader entry point
 */
.global _start
_start:
	/* set machine to a known state */
	li 3, MSR_DE
	mtmsr 3
	isync

	/* setup exceptions */
	lis 3, exception_base@h
	mtspr SPRN_IVPR, 3
	li 3, exc_Critical_Input@l
	mtspr SPRN_IVOR3, 3
	li 3, exc_Machine_Check@l
	mtspr SPRN_IVOR1, 3
	li 3, exc_Data_Storage@l
	mtspr SPRN_IVOR2, 3
	li 3, exc_Instruction_Storage@l
	mtspr SPRN_IVOR3, 3
	li 3, exc_External_Input@l
	mtspr SPRN_IVOR4, 3
	li 3, exc_Alignment@l
	mtspr SPRN_IVOR5, 3
	li 3, exc_Program@l
	mtspr SPRN_IVOR6, 3
#if defined(POWER_CAT_FP)
	li 3, exc_Floating_Point_Unavailable@l
	mtspr SPRN_IVOR7, 3
#endif
	li 3, exc_System_Call@l
	mtspr SPRN_IVOR8, 3
#if defined(POWER_IVOR9)
	li 3, exc_Auxiliary_Processor_Unavailable@l
	mtspr SPRN_IVOR9, 3
#endif
	li 3, exc_Decrementer@l
	mtspr SPRN_IVOR10, 3
	li 3, exc_Fixed_Interval_Timer@l
	mtspr SPRN_IVOR11, 3
	li 3, exc_Watchdog_Timer@l
	mtspr SPRN_IVOR12, 3
	li 3, exc_Data_TLB_Error@l
	mtspr SPRN_IVOR13, 3
	li 3, exc_Instruction_TLB_Error@l
	mtspr SPRN_IVOR14, 3
	li 3, exc_Debug@l
	mtspr SPRN_IVOR15, 3
#if defined(POWER_CAT_SP)
	li 3, exc_SPE_EFP_Vector_Unavailable@l
	mtspr SPRN_IVOR32, 3
	li 3, exc_EFP_Data@l
	mtspr SPRN_IVOR33, 3
	li 3, exc_EFP_Round@l
	mtspr SPRN_IVOR34, 3
#endif

#if defined(POWER_CAT_SP) || defined(POWER_CAT_V)
	/* enable access to FPU */
	lis 3, (MSR_DE | MSR_SPV)@h
	ori 3, 3, (MSR_DE | MSR_SPV)@l
	mtmsr 3
	isync
#endif

#if defined(POWER_CAT_FP)
	/* enable access to FPU */
	lis 3, (MSR_DE | MSR_FP)@l
	ori 3, 3, (MSR_DE | MSR_FP)@h
	mtmsr 3
	isync
#endif

	/* do machine initialisation - on return we assume:
	 *  - clocks are initialised
	 *  - cache is initialised
	 *  - direct (phys = virt) mapping for kernel and loader memory */
	bl machine_init

	/* setup stack */
	lis 1, __stack_top@h
	ori 1, 1, (__stack_top - 16)@l

	/* jump to crt_init */
	b crt_init

#define EXCEPTION(num, name) \
.align 4; \
name:; \
	li 3, num; \
	b machine_exception

.align 4
exception_base:
	EXCEPTION(0, exc_Critical_Input)
	EXCEPTION(1, exc_Machine_Check)
	EXCEPTION(2, exc_Data_Storage)
	EXCEPTION(3, exc_Instruction_Storage)
	EXCEPTION(4, exc_External_Input)
	EXCEPTION(5, exc_Alignment)
	EXCEPTION(6, exc_Program)
#if defined(POWER_CAT_FP)
	EXCEPTION(7, exc_Floating_Point_Unavailable)
#endif
	EXCEPTION(8, exc_System_Call)
#if defined(POWER_IVOR9)
	EXCEPTION(9, exc_Auxiliary_Processor_Unavailable)
#endif
	EXCEPTION(10, exc_Decrementer)
	EXCEPTION(11, exc_Fixed_Interval_Timer)
	EXCEPTION(12, exc_Watchdog_Timer)
	EXCEPTION(13, exc_Data_TLB_Error)
	EXCEPTION(14, exc_Instruction_TLB_Error)
	EXCEPTION(15, exc_Debug)
#if defined(POWER_CAT_SP)
	EXCEPTION(32, exc_SPE_EFP_Vector_Unavailable)
	EXCEPTION(33, exc_EFP_Data)
	EXCEPTION(34, exc_EFP_Round)
#endif
