#pragma once

/*
 * e500v2/cpu.h
 *
 * See PowerPC e500 Core Family Reference Manual E500CORERM Rev. 1, 4/2005
 */

#define POWER_CAT_E
#define POWER_CAT_SP
#define POWER_CAT_ATB
#define POWER_CAT_E_PM
#define POWER_IMPL_MSR \
	bitfield::ppcbit<S, bool, 45> WE; \
	bitfield::ppcbit<S, bool, 53> UBLE;
#define POWER_MAS7
#include "../isa207b.h"
#include "../intrinsics.h"

/*
 * L1 Cache Control and Status Register 0
 */
union L1CSR0 {
	static constexpr auto SPRN = 1010;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 47> CECE;
	bitfield::ppcbit<S, bool, 48> CEI;
	bitfield::ppcbit<S, bool, 52> CSLC;
	bitfield::ppcbit<S, bool, 53> CUL;
	bitfield::ppcbit<S, bool, 54> CLO;
	bitfield::ppcbit<S, bool, 55> CLFC;
	bitfield::ppcbit<S, bool, 62> CFI;
	bitfield::ppcbit<S, bool, 63> CE;
};
static_assert(sizeof(L1CSR0) == 4);

/*
 * L1 Cache Control and Status Register 1
 */
union L1CSR1 {
	static constexpr auto SPRN = 1011;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 47> ICECE;
	bitfield::ppcbit<S, bool, 48> ICEI;
	bitfield::ppcbit<S, bool, 52> ICSLC;
	bitfield::ppcbit<S, bool, 53> ICUL;
	bitfield::ppcbit<S, bool, 54> ICLO;
	bitfield::ppcbit<S, bool, 55> ICLFC;
	bitfield::ppcbit<S, bool, 62> ICFI;
	bitfield::ppcbit<S, bool, 63> ICE;
};
static_assert(sizeof(L1CSR1) == 4);

/*
 * Hardware Implementation Dependent Register 0
 */
union HID0 {
	static constexpr auto SPRN = 1008;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 32> EMCP;
	bitfield::ppcbit<S, bool, 40> DOZE;
	bitfield::ppcbit<S, bool, 41> NAP;
	bitfield::ppcbit<S, bool, 42> SLEEP;
	bitfield::ppcbit<S, bool, 49> TBEN;
	bitfield::ppcbit<S, bool, 50> SEL_TBCLK;
	bitfield::ppcbit<S, bool, 56> EN_MAS7_UPDATE;
	bitfield::ppcbit<S, bool, 57> DCFA;
	bitfield::ppcbit<S, bool, 63> NOPTI;
};
static_assert(sizeof(HID0) == 4);

/*
 * Hardware Implementation Dependent Register 1
 */
union HID1 {
	static constexpr auto SPRN = 1009;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, bool, 32, 37> PLL_CFG;
	bitfield::ppcbit<S, bool, 46> RFXE;
	bitfield::ppcbit<S, bool, 48> R1DPE;
	bitfield::ppcbit<S, bool, 49> R2DPE;
	bitfield::ppcbit<S, bool, 50> ASTME;
	bitfield::ppcbit<S, bool, 51> ABE;
	bitfield::ppcbit<S, bool, 53> MPXTT;
	bitfield::ppcbit<S, bool, 56> ATS;
	bitfield::ppcbit<S, bool, 60> MID;
};
static_assert(sizeof(HID1) == 4);

/*
 * SPRs Specific to e500
 */
// USPRG0	SPRN_VRSAVE
// BBEAR	513
// BBTAR	514
// L1CFG0	515
// L1CFG1	516
// MCSR		572
// MCAR		573
// PID1		633
// PID2		634
// BUCSR	1013
// SVR		1023

#warning deleteme
#if 0
#include <stdint.h>

#include <type_traits>

/*
 * L1 Cache Configuration Register 0
 */
union L1CFG0 {
	struct {
		uint32_t CARCH : 2;
		uint32_t CWPA : 1;
		uint32_t CFAHA : 1;
		uint32_t CFISW : 1;
		uint32_t : 2;
		uint32_t CBSIZE : 2;
		uint32_t CREPL : 2;
		uint32_t CLA : 1;
		uint32_t CPA : 1;
		uint32_t CNWAY : 8;
		uint32_t CSIZE : 11;
	};
	uint32_t r;
};

/*
 * L1 Cache Configuration Register 1
 */
union L1CFG1 {
	struct {
		uint32_t : 7;
		uint32_t ICBSIZE : 2;
		uint32_t ICREPL : 2;
		uint32_t ICLA : 1;
		uint32_t ICPA : 1;
		uint32_t ICNWAY : 8;
		uint32_t ICSIZE : 11;
	};
	uint32_t r;
};

#endif
