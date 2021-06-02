#include "../asm_def_common.h"

#include <booke/locore.h>
#include <cpu.h>
#include <cstddef>
#include <thread.h>

/*
 * Generic PowerPC Book E Definitions
 */
void asm_def()
{
	text("#pragma once");
	text();
	text("/*");
	text(" * asm_def.h - Automatically generated file. Do not edit.");
	text(" *");
	text(" * Generic PowerPC Book E Definitions");
	text(" */");
	text();
	text("/* Configuration */");
#if defined(POWER_CAT_SP)
	text("#define POWER_CAT_SP");
#endif
#if defined(POWER_CAT_V)
	text("#define POWER_CAT_V");
#endif
#if defined(POWER_CAT_FP)
	text("#define POWER_CAT_FP");
#endif
#if defined(POWER_IVOR9)
	text("#define POWER_IVOR9");
#endif
#if defined(POWER_CAT_E_ED)
	text("#define POWER_CAT_E_ED");
#endif
#if defined(POWER_MACHINE_CHECK_EXTENSION)
	text("#define POWER_MACHINE_CHECK_EXTENSION");
#endif
	text();
	text("/* Machine State Register */");
	MSR msr;
#if defined(POWER_CAT_SP) || defined(POWER_CAT_V)
	define_bit("MSR_SPV", msr.SPV.offset);
#endif
	define_bit("MSR_CE", msr.CE.offset);
	define_bit("MSR_EE", msr.EE.offset);
	define_bit("MSR_PR", msr.PR.offset);
#if defined(POWER_CAT_FP)
	define_bit("MSR_FP", msr.FP.offset);
#endif
	define_bit("MSR_ME", msr.ME.offset);
	define_bit("MSR_DE", msr.DE.offset);
	define_bit("MSR_IS", msr.IS.offset);
	define_bit("MSR_DS", msr.DS.offset);
	text();
	text("/* MMU Asssist Register 0 */");
	MAS0 mas0;
	define_val("SPRN_MAS0", MAS0::SPRN);
	define_shift("MAS0_TLBSEL", mas0.TLBSEL.offset);
	define_shift("MAS0_ESEL", mas0.ESEL.offset);
	text();
	text("/* MMU Asssist Register 1 */");
	MAS1 mas1;
	define_val("SPRN_MAS1", MAS1::SPRN);
	define_bit("MAS1_V", mas1.V.offset);
	define_bit("MAS1_IPROT", mas1.IPROT.offset);
	define_shift("MAS1_TID", mas1.TID.offset);
	define_shift("MAS1_TS", mas1.TS.offset);
	define_shift("MAS1_TSIZE", mas1.TSIZE.offset);
	text();
	text("/* MMU Asssist Register 2 */");
	MAS2 mas2;
	define_val("SPRN_MAS2", MAS2::SPRN);
	define_shift("MAS2_EPN", mas2.EPN.offset);
	define_bit("MAS2_W", mas2.W.offset);
	define_bit("MAS2_I", mas2.I.offset);
	define_bit("MAS2_M", mas2.M.offset);
	define_bit("MAS2_G", mas2.G.offset);
	define_bit("MAS2_E", mas2.E.offset);
	text();
	text("/* MMU Asssist Register 3 */");
	MAS3 mas3;
	define_val("SPRN_MAS3", MAS3::SPRN);
	define_shift("MAS3_RPN", mas3.RPN.offset);
	define_shift("MAS3_U", mas3.U.offset);
	define_bit("MAS3_UX", mas3.UX.offset);
	define_bit("MAS3_SX", mas3.SX.offset);
	define_bit("MAS3_UW", mas3.UW.offset);
	define_bit("MAS3_SW", mas3.SW.offset);
	define_bit("MAS3_UR", mas3.UR.offset);
	define_bit("MAS3_SR", mas3.SR.offset);
	text();
	text("/* MMU Asssist Register 7 */");
	define_val("SPRN_MAS7", MAS7::SPRN);
	define_shift("MAS7_RPNU", 0);
	text();
	text("/* Convert an address to a page number for use in EPN and RPN fields */");
	__asm__("\n#__OUT__#define PAGE_NUMBER_L(addr) (((addr) >> %0) & %1)" :: "n"(mas2.EPN.offset), "n"(mas2.EPN.max));
	text("#define PAGE_NUMBER_U(addr) ((addr) >> 32)");
	text();
	text("/* Interrupt Vector Prefix Register */");
	define_val("SPRN_IVPR", IVPR::SPRN);
	text();
	text("/* Interrupt Vector Offset Registers */");
	define_val("SPRN_IVOR0", IVOR0::SPRN);
	define_val("SPRN_IVOR1", IVOR1::SPRN);
	define_val("SPRN_IVOR2", IVOR2::SPRN);
	define_val("SPRN_IVOR3", IVOR3::SPRN);
	define_val("SPRN_IVOR4", IVOR4::SPRN);
	define_val("SPRN_IVOR5", IVOR5::SPRN);
	define_val("SPRN_IVOR6", IVOR6::SPRN);
#if defined(POWER_CAT_FP)
	define_val("SPRN_IVOR7", IVOR7::SPRN);
#endif
	define_val("SPRN_IVOR8", IVOR8::SPRN);
#if defined(POWER_IVOR9)
	define_val("SPRN_IVOR9", IVOR9::SPRN);
#endif
	define_val("SPRN_IVOR10", IVOR10::SPRN);
	define_val("SPRN_IVOR11", IVOR11::SPRN);
	define_val("SPRN_IVOR12", IVOR12::SPRN);
	define_val("SPRN_IVOR13", IVOR13::SPRN);
	define_val("SPRN_IVOR14", IVOR14::SPRN);
	define_val("SPRN_IVOR15", IVOR15::SPRN);
#if defined(POWER_CAT_SP)
	define_val("SPRN_IVOR32", IVOR32::SPRN);
	define_val("SPRN_IVOR33", IVOR33::SPRN);
	define_val("SPRN_IVOR34", IVOR34::SPRN);
#endif
	text();
	text("/* Save Restore Registers */");
	define_val("SPRN_SRR0", SRR0::SPRN);
	define_val("SPRN_SRR1", SRR1::SPRN);
	define_val("SPRN_CSRR0", CSRR0::SPRN);
	define_val("SPRN_CSRR1", CSRR1::SPRN);
#if defined(POWER_MACHINE_CHECK_EXTENSION)
	define_val("SPRN_MCSRR0", MCSRR0::SPRN);
	define_val("SPRN_MCSRR1", MCSRR1::SPRN);
#endif
#if defined(POWER_CAT_E_ED)
	define_val("SPRN_DSRR0", DSRR0::SPRN);
	define_val("SPRN_DSRR1", DSRR1::SPRN);
#endif
	text();
	text("/* Other Special Purpose Registers */");
	define_val("SPRN_ESR", ESR::SPRN);
	define_val("SPRN_DEAR", DEAR::SPRN);
#if defined(POWER_CAT_SP)
	define_val("SPRN_SPEFSCR", SPEFSCR::SPRN);
#endif
	text();
	text("/* Scratch Registers */");
	define_val("SPRN_BASE_SCRATCH0", BASE_SCRATCH0::SPRN);
	define_val("SPRN_BASE_SCRATCH1", BASE_SCRATCH1::SPRN);
	define_val("SPRN_BASE_SCRATCH2", BASE_SCRATCH2::SPRN);
	define_val("SPRN_CRITICAL_SCRATCH", CRITICAL_SCRATCH::SPRN);
	define_val("SPRN_MACHINE_CHECK_SCRATCH", MACHINE_CHECK_SCRATCH::SPRN);
#if defined(POWER_CAT_E_ED)
	define_val("SPRN_DEBUG_SCRATCH", DEBUG_SCRATCH::SPRN);
#endif
	text();
	text("/* IRQ_NESTING */");
	define_val("SPRN_IRQ_NESTING", IRQ_NESTING::SPRN);
	text();
	text("/* cpu_data */");
	define_val("SPRN_CPU_DATA", CPU_DATA::SPRN);
	define_val("CPU_DATA_BASE_IRQ_STACK", offsetof(cpu_data, base_irq_stack));
	define_val("CPU_DATA_CRITICAL_IRQ_STACK", offsetof(cpu_data, critical_irq_stack));
#if defined(POWER_MACHINE_CHECK_EXTENSION)
	define_val("CPU_DATA_MACHINE_CHECK_IRQ_STACK", offsetof(cpu_data, machine_check_irq_stack));
#endif
#if defined(POWER_CAT_E_ED)
	define_val("CPU_DATA_DEBUG_STACK", offsetof(cpu_data, debug_irq_stack));
#endif
	text();
	text("/* min_frame */");
	define_val("MIN_FRAME_SIZE", sizeof(min_frame));
	text();
	text("/* irq_frame */");
	define_val("IRQ_FRAME_SIZE", sizeof(irq_frame));
	define_val("IRQ_FRAME_R0", offsetof(irq_frame, r0));
	define_val("IRQ_FRAME_R2", offsetof(irq_frame, r2));
	define_val("IRQ_FRAME_R3", offsetof(irq_frame, r3));
	define_val("IRQ_FRAME_R4", offsetof(irq_frame, r4));
	define_val("IRQ_FRAME_R5", offsetof(irq_frame, r5));
	define_val("IRQ_FRAME_R6", offsetof(irq_frame, r6));
	define_val("IRQ_FRAME_R7", offsetof(irq_frame, r7));
	define_val("IRQ_FRAME_R8", offsetof(irq_frame, r8));
	define_val("IRQ_FRAME_R9", offsetof(irq_frame, r9));
	define_val("IRQ_FRAME_R10", offsetof(irq_frame, r10));
	define_val("IRQ_FRAME_R11", offsetof(irq_frame, r11));
	define_val("IRQ_FRAME_R12", offsetof(irq_frame, r12));
	define_val("IRQ_FRAME_R13", offsetof(irq_frame, r13));
#if defined(POWER_CAT_SP)
	define_val("IRQ_FRAME_SPEFSCR", offsetof(irq_frame, spefscr));
#endif
	define_val("IRQ_FRAME_XER", offsetof(irq_frame, xer));
	define_val("IRQ_FRAME_CTR", offsetof(irq_frame, ctr));
	define_val("IRQ_FRAME_LR", offsetof(irq_frame, lr));
	define_val("IRQ_FRAME_CR", offsetof(irq_frame, cr));
	define_val("IRQ_FRAME_NIP", offsetof(irq_frame, nip));
	define_val("IRQ_FRAME_MSR", offsetof(irq_frame, msr));
	define_val("IRQ_FRAME_ESR", offsetof(irq_frame, esr));
	define_val("IRQ_FRAME_DEAR", offsetof(irq_frame, dear));
	text();
	text("/* context_frame */");
	define_val("CONTEXT_FRAME_SIZE", sizeof(context_frame));
#if defined(POWER_CAT_SP)
	define_val("CONTEXT_FRAME_SPEVALID", offsetof(context_frame, spevalid));
	define_val("CONTEXT_FRAME_SPEFSCR", offsetof(context_frame, spefscr));
#endif
	define_val("CONTEXT_FRAME_XER", offsetof(context_frame, xer));
	define_val("CONTEXT_FRAME_CTR", offsetof(context_frame, ctr));
	define_val("CONTEXT_FRAME_LR", offsetof(context_frame, lr));
	define_val("CONTEXT_FRAME_CR", offsetof(context_frame, cr));
	define_val("CONTEXT_FRAME_NIP", offsetof(context_frame, nip));
	define_val("CONTEXT_FRAME_MSR", offsetof(context_frame, msr));
	define_val("CONTEXT_FRAME_R0", offsetof(context_frame, r[0]));
	define_val("CONTEXT_FRAME_R1", offsetof(context_frame, r[1]));
	define_val("CONTEXT_FRAME_R2", offsetof(context_frame, r[2]));
	define_val("CONTEXT_FRAME_R3", offsetof(context_frame, r[3]));
	define_val("CONTEXT_FRAME_R4", offsetof(context_frame, r[4]));
	define_val("CONTEXT_FRAME_R5", offsetof(context_frame, r[5]));
	define_val("CONTEXT_FRAME_R6", offsetof(context_frame, r[6]));
	define_val("CONTEXT_FRAME_R7", offsetof(context_frame, r[7]));
	define_val("CONTEXT_FRAME_R8", offsetof(context_frame, r[8]));
	define_val("CONTEXT_FRAME_R9", offsetof(context_frame, r[9]));
	define_val("CONTEXT_FRAME_R10", offsetof(context_frame, r[10]));
	define_val("CONTEXT_FRAME_R11", offsetof(context_frame, r[11]));
	define_val("CONTEXT_FRAME_R12", offsetof(context_frame, r[12]));
	define_val("CONTEXT_FRAME_R13", offsetof(context_frame, r[13]));
	define_val("CONTEXT_FRAME_R14", offsetof(context_frame, r[14]));
	define_val("CONTEXT_FRAME_R15", offsetof(context_frame, r[15]));
	define_val("CONTEXT_FRAME_R16", offsetof(context_frame, r[16]));
	define_val("CONTEXT_FRAME_R17", offsetof(context_frame, r[17]));
	define_val("CONTEXT_FRAME_R18", offsetof(context_frame, r[18]));
	define_val("CONTEXT_FRAME_R19", offsetof(context_frame, r[19]));
	define_val("CONTEXT_FRAME_R20", offsetof(context_frame, r[20]));
	define_val("CONTEXT_FRAME_R21", offsetof(context_frame, r[21]));
	define_val("CONTEXT_FRAME_R22", offsetof(context_frame, r[22]));
	define_val("CONTEXT_FRAME_R23", offsetof(context_frame, r[23]));
	define_val("CONTEXT_FRAME_R24", offsetof(context_frame, r[24]));
	define_val("CONTEXT_FRAME_R25", offsetof(context_frame, r[25]));
	define_val("CONTEXT_FRAME_R26", offsetof(context_frame, r[26]));
	define_val("CONTEXT_FRAME_R27", offsetof(context_frame, r[27]));
	define_val("CONTEXT_FRAME_R28", offsetof(context_frame, r[28]));
	define_val("CONTEXT_FRAME_R29", offsetof(context_frame, r[29]));
	define_val("CONTEXT_FRAME_R30", offsetof(context_frame, r[30]));
	define_val("CONTEXT_FRAME_R31", offsetof(context_frame, r[31]));
#if defined(POWER_CAT_SP)
	define_val("CONTEXT_FRAME_ACC", offsetof(context_frame, acc));
	define_val("CONTEXT_FRAME_RH0", offsetof(context_frame, rh[0]));
	define_val("CONTEXT_FRAME_RH1", offsetof(context_frame, rh[1]));
	define_val("CONTEXT_FRAME_RH2", offsetof(context_frame, rh[2]));
	define_val("CONTEXT_FRAME_RH3", offsetof(context_frame, rh[3]));
	define_val("CONTEXT_FRAME_RH4", offsetof(context_frame, rh[4]));
	define_val("CONTEXT_FRAME_RH5", offsetof(context_frame, rh[5]));
	define_val("CONTEXT_FRAME_RH6", offsetof(context_frame, rh[6]));
	define_val("CONTEXT_FRAME_RH7", offsetof(context_frame, rh[7]));
	define_val("CONTEXT_FRAME_RH8", offsetof(context_frame, rh[8]));
	define_val("CONTEXT_FRAME_RH9", offsetof(context_frame, rh[9]));
	define_val("CONTEXT_FRAME_RH10", offsetof(context_frame, rh[10]));
	define_val("CONTEXT_FRAME_RH11", offsetof(context_frame, rh[11]));
	define_val("CONTEXT_FRAME_RH12", offsetof(context_frame, rh[12]));
	define_val("CONTEXT_FRAME_RH13", offsetof(context_frame, rh[13]));
	define_val("CONTEXT_FRAME_RH14", offsetof(context_frame, rh[14]));
	define_val("CONTEXT_FRAME_RH15", offsetof(context_frame, rh[15]));
	define_val("CONTEXT_FRAME_RH16", offsetof(context_frame, rh[16]));
	define_val("CONTEXT_FRAME_RH17", offsetof(context_frame, rh[17]));
	define_val("CONTEXT_FRAME_RH18", offsetof(context_frame, rh[18]));
	define_val("CONTEXT_FRAME_RH19", offsetof(context_frame, rh[19]));
	define_val("CONTEXT_FRAME_RH20", offsetof(context_frame, rh[20]));
	define_val("CONTEXT_FRAME_RH21", offsetof(context_frame, rh[21]));
	define_val("CONTEXT_FRAME_RH22", offsetof(context_frame, rh[22]));
	define_val("CONTEXT_FRAME_RH23", offsetof(context_frame, rh[23]));
	define_val("CONTEXT_FRAME_RH24", offsetof(context_frame, rh[24]));
	define_val("CONTEXT_FRAME_RH25", offsetof(context_frame, rh[25]));
	define_val("CONTEXT_FRAME_RH26", offsetof(context_frame, rh[26]));
	define_val("CONTEXT_FRAME_RH27", offsetof(context_frame, rh[27]));
	define_val("CONTEXT_FRAME_RH28", offsetof(context_frame, rh[28]));
	define_val("CONTEXT_FRAME_RH29", offsetof(context_frame, rh[29]));
	define_val("CONTEXT_FRAME_RH30", offsetof(context_frame, rh[30]));
	define_val("CONTEXT_FRAME_RH31", offsetof(context_frame, rh[31]));
#endif
	text();
	text("/* thread */");
	define_val("THREAD_KSTACK", offsetof(thread, kstack));
	define_val("THREAD_CTX_KSP", offsetof(thread, ctx.ksp));

	text();
	text("/* locore.h */");
#if defined(POWER_CAT_SP)
	define_val("KERNEL_SPEFSCR", kernel_spefscr);
#endif
}
