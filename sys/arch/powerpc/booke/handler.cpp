#include <booke/locore.h>
#include <compiler.h>
#include <debug.h>
#include <sections.h>

/*
 * exc_Decrementer
 *
 * If the systick driver is in use it will handle exc_Decrementer.
 */
extern "C" void
unhandled_Decrementer()
{
	panic("Unhandled Decrementer");
}
weak_alias(unhandled_Decrementer, exc_Decrementer);

/*
 * exc_External_Input
 */
extern "C" void
unhandled_External_Input()
{
	panic("Unhandled External Input");
}
weak_alias(unhandled_External_Input, exc_External_Input);

/*
 * irq_handler_full_context - handler for full context IRQs
 */
extern "C" __fast_text void
irq_handler_full_context(unsigned n, context_frame *ctx)
{
	switch (n) {
	case 0:
		/* Critical_Input */
		panic("Critical Input");
	case 1:
		/* Machine_Check */
		panic("Machine Check");
	case 15:
		/* Debug */
		panic("Debug");
	case 12:
		/* Watchdog_Timer*/
		panic("Watchdog Timer");
#if defined(POWER_CAT_SP)
	case 33:
		/* EFP_Data */
		panic("EFP Data");
	case 34:
		/* EFP_Round */
		panic("EFP Round");
#endif
	default:
		dbg("IRQ %u not handled\n", n);
		panic("IRQ not handled");
	}
}

/*
 * irq_handler_min_context - handler for minimum context IRQs
 */
extern "C" __fast_text void
irq_handler_min_context(unsigned n, irq_frame *ctx)
{
	switch (n) {
	case 2:
		/* Data_Storage */
		panic("Data Storage");
	case 3:
		/* Instruction_Storage */
		panic("Instruction Storage");
	case 4:
		/* External_Input */
		exc_External_Input();
		return;
	case 5:
		/* Alignment */
		panic("Alignment");
	case 6:
		/* Program */
		panic("Program");
#if defined(POWER_CAT_FP)
	case 7:
		/* Floating_Point_Unavailable */
		panic("Floating Point Unavailable");
#endif
#if defined(POWER_IVOR9)
	case 9:
		/* Auxiliary_Processor_Unavailable */
		panic("Auxiliary Processor Unavailable");
#endif
	case 10:
		/* Decrementer */
		exc_Decrementer();
		return;
	case 11:
		/* Fixed_Interval_Timer */
		panic("Fixed Interval Timer");
	case 13:
		/* Data_TLB_Error */
		panic("Data TLB Error");
	case 14:
		/* Instruction_TLB_Error */
		panic("Instruction TLB Error");
#if defined(POWER_CAT_SP)
	case 32:
		/* SPE_EFP_Vector_Unavailable */
		ctx->msr |= decltype(MSR::SPV)(true).r;
		return;
#endif
	default:
		dbg("IRQ %u not handled\n", n);
		panic("IRQ not handled");
	}
}
