#include "../asm_def_common.h"

#include <cpu.h>

/*
 * Freescale PowerPC Book E Definitions
 */
void asm_def()
{
	text("#pragma once");
	text();
	text("/*");
	text(" * asm_def.h - Automatically generated file. Do not edit.");
	text(" *");
	text(" * Freescale PowerPC Book E Definitions");
	text(" */");
	text();
	text("/* L1 Cache Control and Status Register 0 */");
	L1CSR0 l1csr0;
	define_val("SPRN_L1CSR0", L1CSR0::SPRN);
	define_bit("L1CSR0_CECE", l1csr0.CECE.offset);
	define_bit("L1CSR0_CEI", l1csr0.CEI.offset);
	define_bit("L1CSR0_CSLC", l1csr0.CSLC.offset);
	define_bit("L1CSR0_CUL", l1csr0.CUL.offset);
	define_bit("L1CSR0_CLO", l1csr0.CLO.offset);
	define_bit("L1CSR0_CLFC", l1csr0.CLFC.offset);
	define_bit("L1CSR0_CFI", l1csr0.CFI.offset);
	define_bit("L1CSR0_CE", l1csr0.CE.offset);
	text();
	text("/* L1 Cache Control and Status Register 1 */");
	L1CSR1 l1csr1;
	define_val("SPRN_L1CSR1", L1CSR1::SPRN);
	define_bit("L1CSR1_ICECE", l1csr1.ICECE.offset);
	define_bit("L1CSR1_ICEI", l1csr1.ICEI.offset);
	define_bit("L1CSR1_ICSLC", l1csr1.ICSLC.offset);
	define_bit("L1CSR1_ICUL", l1csr1.ICUL.offset);
	define_bit("L1CSR1_ICLO", l1csr1.ICLO.offset);
	define_bit("L1CSR1_ICLFC", l1csr1.ICLFC.offset);
	define_bit("L1CSR1_ICFI", l1csr1.ICFI.offset);
	define_bit("L1CSR1_ICE", l1csr1.ICE.offset);
}
