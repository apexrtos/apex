#include "../asm_def_common.h"

#include <cpu.h>

/*
 * e500v2 Definitions
 */
void asm_def()
{
	text("#pragma once");
	text();
	text("/*");
	text(" * asm_def.h - Automatically generated file. Do not edit.");
	text(" *");
	text(" * e500v2 Definitions");
	text(" */");
	text();
	text("/* Hardware Implementation Dependent Register 0 */");
	HID0 hid0;
	define_val("SPRN_HID0", HID0::SPRN);
	define_bit("HID0_EMCP", hid0.EMCP.offset);
	define_bit("HID0_DOZE", hid0.DOZE.offset);
	define_bit("HID0_NAP", hid0.NAP.offset);
	define_bit("HID0_SLEEP", hid0.SLEEP.offset);
	define_bit("HID0_TBEN", hid0.TBEN.offset);
	define_bit("HID0_SEL_TBCLK", hid0.SEL_TBCLK.offset);
	define_bit("HID0_EN_MAS7_UPDATE", hid0.EN_MAS7_UPDATE.offset);
	define_bit("HID0_DCFA", hid0.DCFA.offset);
	define_bit("HID0_NOPTI", hid0.NOPTI.offset);
	text();
	text("/* Hardware Implementation Dependent Register 1 */");
	HID1 hid1;
	define_val("SPRN_HID1", HID1::SPRN);
	define_shift("HID1_PLL_CFG", hid1.PLL_CFG.offset);
	define_bit("HID2_RFXE", hid1.RFXE.offset);
	define_bit("HID2_R1DPE", hid1.R1DPE.offset);
	define_bit("HID2_R2DPE", hid1.R2DPE.offset);
	define_bit("HID2_ASTME", hid1.ASTME.offset);
	define_bit("HID2_ABE", hid1.ABE.offset);
	define_bit("HID2_MPXTT", hid1.MPXTT.offset);
	define_bit("HID2_ATS", hid1.ATS.offset);
	define_bit("HID2_MID", hid1.MID.offset);
}
