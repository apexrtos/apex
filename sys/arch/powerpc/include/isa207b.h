#pragma once

/*
 * isa207b.h
 *
 * See Power ISA 2.07B (January 30, 2018)
 */

#include "../bitfield.h"
#include <cstdint>

/*
 * Configuration
 *
 * POWER_CAT_64
 * POWER_CAT_ATB
 * POWER_CAT_E
 * POWER_CAT_EC
 * POWER_CAT_ECL
 * POWER_CAT_EXP
 * POWER_CAT_E_CD
 * POWER_CAT_E_ED
 * POWER_CAT_E_HV
 * POWER_CAT_E_HV_LRAT
 * POWER_CAT_E_MT
 * POWER_CAT_E_PD
 * POWER_CAT_E_PM
 * POWER_CAT_E_PT
 * POWER_CAT_FP
 * POWER_CAT_PC
 * POWER_CAT_S
 * POWER_CAT_SP
 * POWER_CAT_STM
 * POWER_CAT_TM
 * POWER_CAT_V
 * POWER_CAT_VLE
 * POWER_CAT_VSX
 * POWER_IMPL_MSR
 * POWER_IVOR9
 * POWER_MAS2U
 * POWER_MAS2_ACM
 * POWER_MAS7
 * POWER_MAV_2
 * POWER_MCIVPR
 * POWER_MACHINE_CHECK_EXTENSION
 * POWER_PPR32
 * POWER_SPRG8
 * POWER_TLB2CFG
 * POWER_TLB3CFG
 */

/*
 * B - Base
 */

/* Fixed-Point Exception Register */
union XER {
	static constexpr auto SPRN = 1;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 32> SO;
	bitfield::ppcbit<S, bool, 33> OV;
	bitfield::ppcbit<S, bool, 34> CA;
};

/* Link Register */
union LR {
	static constexpr auto SPRN = 8;
	uint32_t r;
};

/* Count Register */
union CTR {
	static constexpr auto SPRN = 9;
	uint32_t r;
};

/* Decrementer */
union DEC {
	static constexpr auto SPRN = 22;
	uint32_t r;
};

/* Machine Status Save/Restore Register 0 */
union SRR0 {
	static constexpr auto SPRN = 26;
	uint32_t r;
};

/* Machine Status Save/Restore Register 1 */
union SRR1 {
	static constexpr auto SPRN = 27;
	uint32_t r;
};

// VRSAVE	256
// TB_UNPRIV	268
// TBU_UNPRIV	269

/* Software-Use SPR 0 */
union SPRG0 {
	static constexpr auto SPRN = 272;
	uint32_t r;
};

/* Software-Use SPR 1 */
union SPRG1 {
	static constexpr auto SPRN = 273;
	uint32_t r;
};

/* Software-Use SPR 2 */
union SPRG2 {
	static constexpr auto SPRN = 274;
	uint32_t r;
};

/* Software-Use SPR 3 */
union SPRG3 {
	static constexpr auto SPRN = 275;
	static constexpr auto SPRN_UNPRIV = 259;
	uint32_t r;
};

/* Time Base Lower */
union TBL {
	static constexpr auto SPRN = 284;
	uint32_t r;
};

/* Time Base Upper */
union TBU {
	static constexpr auto SPRN = 285;
	uint32_t r;
};

// PVR		287
#if defined(POWER_PPR32)
// PPR32	898
#endif

/*
 * S - Server
 */
#if defined(POWER_CAT_S)
// AMR		13
// DSISR	18
// DAR		19
// SDR1		25
// CFAR		28
// AMR		29
// IAMR		61
// CTRL		136
// CTRL		152
// FSCR		153
// UAMOR	157
// PSPB		159
// DPDES	176
// DHDES	177
// DAWR0	180
// RPR		186
// CIABR	187
// DAWRX0	188
// HFSCR	190
// CIR		283
// TBU40	286
// HSPRG0	304
// HSPRG1	305
// HDSISR	306
// HDAR		307
// SPURR	308
// PURR		309
// HDEC		310
// RMOR		312
// HRMOR	313
// HSRR0	314
// HSRR1	315
// LPCR		318
// LPIDR	319
// HMER		336
// HMEER	337
// PCR		338
// HEIR		339
// AMOR		349
// TIR		446
// SIER		768
// MMCR2	769
// MMCRA	770
// PMC1		771
// PMC2		772
// PMC3		773
// PMC4		774
// PMC5		775
// PMC6		776
// MMCR0	779
// SIAR		780
// SDAR		781
// MMCR1	782
// SIER		784
// MMCR2	785
// MMCRA	786
// PMC1		787
// PMC2		788
// PMC3		789
// PMC4		790
// PMC5		791
// PMC6		792
// MMCR0	795
// SIAR		796
// SDAR		797
// MMCR1	798
// BESCRS	800
// BESCRSU	801
// BESCRR	802
// BESCRRU	803
// EBBHR	804
// EBBRR	805
// BESCR	806
// TAR		815
// IC		848
// VTB		849
// PPR		896
// PIR		1023
#endif

/*
 * E - Embedded
 */
#if defined(POWER_CAT_E)

/* Machine State Register */
union MSR {
	using S = uint32_t;
	struct { S r; };

#if defined(POWER_CAT_64)
	enum class ComputationMode {
		Bit32 = 0,
		Bit64 = 1,
	};
	bitfield::ppcbit<S, ComputationMode, 32> CM;
#endif
#if defined(POWER_CAT_E_HV)
	enum class GuestState {
		Hypervisor = 0,
		Guest = 1,
	};
	bitfield::ppcbit<S, GuestState, 35> GS;
#endif
#if defined(POWER_CAT_ECL)
	bitfield::ppcbit<S, bool, 37> UCLE;
#endif
#if defined(POWER_CAT_SP) || defined(POWER_CAT_V)
	bitfield::ppcbit<S, bool, 38> SPV;
#endif
#if defined(POWER_CAT_VSX)
	bitfield::ppcbit<S, bool, 40> VSX;
#endif
	bitfield::ppcbit<S, bool, 46> CE;
	bitfield::ppcbit<S, bool, 48> EE;
	enum class ProblemState {
		Supervisor = 0,
		Problem = 1,
	};
	bitfield::ppcbit<S, ProblemState, 49> PR;
#if defined(POWER_CAT_FP)
	bitfield::ppcbit<S, bool, 50> FP;
#endif
	bitfield::ppcbit<S, bool, 51> ME;
#if defined(POWER_CAT_FP)
	bitfield::ppcbit<S, bool, 52> FE0;
#endif
	bitfield::ppcbit<S, bool, 54> DE;
#if defined(POWER_CAT_FP)
	bitfield::ppcbit<S, bool, 55> FE1;
#endif
	bitfield::ppcbit<S, unsigned, 58> IS;
	bitfield::ppcbit<S, unsigned, 59> DS;
#if defined(POWER_CAT_E_PM)
	bitfield::ppcbit<S, bool, 61> PMM;
#endif
	/* Implementation dependent MSR fields */
#if defined(POWER_IMPL_MSR)
	POWER_IMPL_MSR
#endif
};

// PID		48

/* Decrementer Auto-Reload Register */
union DECAR {
	static constexpr auto SPRN = 54;
	uint32_t r;
};

#if defined(POWER_MCIVPR)
// MCIVPR	55
#endif

/* Critical Save/Restore Register 0 */
union CSRR0 {
	static constexpr auto SPRN = 58;
	uint32_t r;
};

/* Critical Save/Restore Register 0 */
union CSRR1 {
	static constexpr auto SPRN = 59;
	uint32_t r;
};

/* Data Exception Address Register */
union DEAR {
	static constexpr auto SPRN = 61;
	uint32_t r;
};

/* Exception Syndrome Register */
union ESR {
	static constexpr auto SPRN = 62;
	uint32_t r;
};

/* Interrupt Vector Prefix Register */
union IVPR {
	static constexpr auto SPRN = 63;
	uint32_t r;
};

/* Software-Use SPR 4 */
union SPRG4 {
	static constexpr auto SPRN = 276;
	static constexpr auto SPRN_UNPRIV = 260;
	uint32_t r;
};

/* Software-Use SPR 5 */
union SPRG5 {
	static constexpr auto SPRN = 277;
	static constexpr auto SPRN_UNPRIV = 261;
	uint32_t r;
};

/* Software-Use SPR 6 */
union SPRG6 {
	static constexpr auto SPRN = 278;
	static constexpr auto SPRN_UNPRIV = 262;
	uint32_t r;
};

/* Software-Use SPR 7 */
union SPRG7 {
	static constexpr auto SPRN = 279;
	static constexpr auto SPRN_UNPRIV = 263;
	uint32_t r;
};

#if defined(POWER_CAT_E_HV)
// CIR		283
#endif
// PIR		286
// DBSR		304
// DBCR0	308
// DBCR1	309
// DBCR2	310
// IAC1		312
// IAC2		313
#if defined(POWER_CAT_E_HV)
// IAC3		314
// IAC4		315
#endif
// DAC1		316
// DAC2		317

/* Timer Status Register */
union TSR {
	static constexpr auto SPRN = 336;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 32> ENW;
	bitfield::ppcbit<S, bool, 33> WIS;
	bitfield::ppcbits<S, unsigned, 34, 35> WRS;
	bitfield::ppcbit<S, bool, 36> DIS;
	bitfield::ppcbit<S, bool, 37> FIS;
};

/* Timer Control Register */
union TCR {
	static constexpr auto SPRN = 340;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 33> WP;
	bitfield::ppcbits<S, unsigned, 34, 35> WRC;
	bitfield::ppcbit<S, bool, 36> WIE;
	bitfield::ppcbit<S, bool, 37> DIE;
	bitfield::ppcbits<S, unsigned, 38, 39> FP;
	bitfield::ppcbit<S, bool, 40> FIE;
	bitfield::ppcbit<S, bool, 41> ARE;
};

#if defined(POWER_CAT_64)
// MAS7_MAS3	372
// MAS0_MAS1	373
#endif

/* Interrupt Vector Offset Registers */
union IVOR0 {
	static constexpr auto SPRN = 400;
	uint32_t r;
};

union IVOR1 {
	static constexpr auto SPRN = 401;
	uint32_t r;
};

union IVOR2 {
	static constexpr auto SPRN = 402;
	uint32_t r;
};

union IVOR3 {
	static constexpr auto SPRN = 403;
	uint32_t r;
};

union IVOR4 {
	static constexpr auto SPRN = 404;
	uint32_t r;
};

union IVOR5 {
	static constexpr auto SPRN = 405;
	uint32_t r;
};

union IVOR6 {
	static constexpr auto SPRN = 406;
	uint32_t r;
};

#if defined(POWER_CAT_FP)
union IVOR7 {
	static constexpr auto SPRN = 407;
	uint32_t r;
};
#endif

union IVOR8 {
	static constexpr auto SPRN = 408;
	uint32_t r;
};

#if defined(POWER_IVOR9)
union IVOR9 {
	static constexpr auto SPRN = 409;
	uint32_t r;
};
#endif

union IVOR10 {
	static constexpr auto SPRN = 410;
	uint32_t r;
};

union IVOR11 {
	static constexpr auto SPRN = 411;
	uint32_t r;
};

union IVOR12 {
	static constexpr auto SPRN = 412;
	uint32_t r;
};

union IVOR13 {
	static constexpr auto SPRN = 413;
	uint32_t r;
};

union IVOR14 {
	static constexpr auto SPRN = 414;
	uint32_t r;
};

union IVOR15 {
	static constexpr auto SPRN = 415;
	uint32_t r;
};

#if defined(POWER_MACHINE_CHECK_EXTENSION)
// MCSRR0	570
// MCSRR1	571
#endif

// MCSR		572
//
#if defined(POWER_SPRG8)
// SPRG8	604
#endif

/* MMU Assist Register 0 */
union MAS0 {
	static constexpr auto SPRN = 624;
	using S = uint32_t;
	struct { S r; };
#if defined(POWER_CAT_E_HV_LRAT)
	bitfield::ppcbit<S, bool, 32> ATSEL;
#endif
	bitfield::ppcbits<S, unsigned, 34, 35> TLBSEL;
	bitfield::ppcbits<S, unsigned, 36, 47> ESEL;
#if defined(POWER_MAV_2)
	bitfield::ppcbit<S, bool, 49> HES;
	bitfield::ppcbits<S, unsigned, 50, 51> WQ;
#endif
	bitfield::ppcbits<S, unsigned, 52, 63> NV;
};
static_assert(sizeof(MAS0) == 4);

/* MMU Assist Register 1 */
union MAS1 {
	static constexpr auto SPRN = 625;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 32> V;
	bitfield::ppcbit<S, bool, 33> IPROT;
	bitfield::ppcbits<S, unsigned, 34, 47> TID;
#if defined(POWER_CAT_E_PT)
	bitfield::ppcbit<S, bool, 50> IND;
#endif
	bitfield::ppcbit<S, unsigned, 51> TS;
	bitfield::ppcbits<S, unsigned, 52, 56> TSIZE;
};
static_assert(sizeof(MAS1) == 4);

/* MMU Assist Register 2 */
union MAS2 {
	static constexpr auto SPRN = 626;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 53> EPN;
#if defined(POWER_MAS2_ACM)
	bitfield::ppcbit<S, bool, 57> ACM;
#endif
#if defined(POWER_CAT_VLE)
	bitfield::ppcbit<S, bool, 58> VLE;
#endif
	bitfield::ppcbit<S, bool, 59> W;
	bitfield::ppcbit<S, bool, 60> I;
	bitfield::ppcbit<S, bool, 61> M;
	bitfield::ppcbit<S, bool, 62> G;
	bitfield::ppcbit<S, bool, 63> E;
};
static_assert(sizeof(MAS2) == 4);

/* MMU Assist Register 3 */
union MAS3 {
	static constexpr auto SPRN = 627;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 53> RPN;
	bitfield::ppcbits<S, unsigned, 54, 57> U;
	bitfield::ppcbit<S, bool, 58> UX;
	bitfield::ppcbit<S, bool, 59> SX;
	bitfield::ppcbit<S, bool, 60> UW;
	bitfield::ppcbit<S, bool, 61> SW;
	bitfield::ppcbit<S, bool, 62> UR;
	bitfield::ppcbit<S, bool, 63> SR;
#if defined(POWER_CAT_E_PT)
	bitfield::ppcbits<S, unsigned, 58, 62> SPSIZE;
#endif
};
static_assert(sizeof(MAS3) == 4);

// MAS4		628
// MAS6		630

#if defined(POWER_MAS2U)
union MAS2U {
	static constexpr auto SPRN = 631;
	uint32_t r;
}
#endif

/* TLB 0 Configuration Register */
union TLB0CFG {
	static constexpr auto SPRN = 688;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 39> ASSOC;
#if defined(POWER_MAV_2)
#if defined(POWER_CAT_E_PT)
	bitfield::ppcbit<S, bool, 45> PT;
	bitfield::ppcbit<S, bool, 46> IND;
#endif
#if defined(POWER_CAT_E_HV_LRAT)
	bitfield::ppcbit<S, bool, 47> GTWE;
#endif
#else
	bitfield::ppcbits<S, unsigned, 40, 43> MINSIZE;
	bitfield::ppcbits<S, unsigned, 44, 47> MAXSIZE;
#endif
	bitfield::ppcbit<S, bool, 48> IPROT;
#if defined(POWER_MAV_2)
	bitfield::ppcbit<S, bool, 50> HES;
#else
	bitfield::ppcbit<S, bool, 49> AVAIL;
#endif
	bitfield::ppcbits<S, unsigned, 52, 63> NENTRY;
};
static_assert(sizeof(TLB0CFG) == 4);

/* TLB 1 Configuration Register */
union TLB1CFG {
	static constexpr auto SPRN = 689;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 39> ASSOC;
#if defined(POWER_MAV_2)
#if defined(POWER_CAT_E_PT)
	bitfield::ppcbit<S, bool, 45> PT;
	bitfield::ppcbit<S, bool, 46> IND;
#endif
#if defined(POWER_CAT_E_HV_LRAT)
	bitfield::ppcbit<S, bool, 47> GTWE;
#endif
#else
	bitfield::ppcbits<S, unsigned, 40, 43> MINSIZE;
	bitfield::ppcbits<S, unsigned, 44, 47> MAXSIZE;
#endif
	bitfield::ppcbit<S, bool, 48> IPROT;
#if defined(POWER_MAV_2)
	bitfield::ppcbit<S, bool, 50> HES;
#else
	bitfield::ppcbit<S, bool, 49> AVAIL;
#endif
	bitfield::ppcbits<S, unsigned, 52, 63> NENTRY;
};
static_assert(sizeof(TLB1CFG) == 4);

/* TLB 2 Configuration Register */
#if defined(POWER_TLB2CFG)
union TLB2CFG {
	static constexpr auto SPRN = 690;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 39> ASSOC;
#if defined(POWER_MAV_2)
#if defined(POWER_CAT_E_PT)
	bitfield::ppcbit<S, bool, 45> PT;
	bitfield::ppcbit<S, bool, 46> IND;
#endif
#if defined(POWER_CAT_E_HV_LRAT)
	bitfield::ppcbit<S, bool, 47> GTWE;
#endif
#else
	bitfield::ppcbits<S, unsigned, 40, 43> MINSIZE;
	bitfield::ppcbits<S, unsigned, 44, 47> MAXSIZE;
#endif
	bitfield::ppcbit<S, bool, 48> IPROT;
#if defined(POWER_MAV_2)
	bitfield::ppcbit<S, bool, 50> HES;
#else
	bitfield::ppcbit<S, bool, 49> AVAIL;
#endif
	bitfield::ppcbits<S, unsigned, 52, 63> NENTRY;
};
static_assert(sizeof(TLB2CFG) == 4);
#endif

/* TLB 3 Configuration Register */
#if defined(POWER_TLB3CFG)
union TLB3CFG {
	static constexpr auto SPRN = 691;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbits<S, unsigned, 32, 39> ASSOC;
#if defined(POWER_MAV_2)
#if defined(POWER_CAT_E_PT)
	bitfield::ppcbit<S, bool, 45> PT;
	bitfield::ppcbit<S, bool, 46> IND;
#endif
#if defined(POWER_CAT_E_HV_LRAT)
	bitfield::ppcbit<S, bool, 47> GTWE;
#endif
#else
	bitfield::ppcbits<S, unsigned, 40, 43> MINSIZE;
	bitfield::ppcbits<S, unsigned, 44, 47> MAXSIZE;
#endif
	bitfield::ppcbit<S, bool, 48> IPROT;
#if defined(POWER_MAV_2)
	bitfield::ppcbit<S, bool, 50> HES;
#else
	bitfield::ppcbit<S, bool, 49> AVAIL;
#endif
	bitfield::ppcbits<S, unsigned, 52, 63> NENTRY;
};
static_assert(sizeof(TLB3CFG) == 4);
#endif

/* MMU Assist Register 7 */
#if defined(POWER_MAS7)
union MAS7 {
	static constexpr auto SPRN = 944;
	uint32_t r;
	uint32_t RPNU;
};
static_assert(sizeof(MAS7) == 4);
#endif

/* MMU Control and Status Register 0 */
union MMUCSR0 {
	static constexpr auto SPRN = 1012;
	using S = uint32_t;
	struct { S r; };
#if !defined(POWER_MAV_2)
	bitfield::ppcbits<S, unsigned, 41, 44> TLB3_PS;
	bitfield::ppcbits<S, unsigned, 45, 48> TLB2_PS;
	bitfield::ppcbits<S, unsigned, 49, 52> TLB1_PS;
	bitfield::ppcbits<S, unsigned, 53, 56> TLB0_PS;
#endif
	bitfield::ppcbit<S, bool, 57> TLB2_FI;
	bitfield::ppcbit<S, bool, 58> TLB3_FI;
	bitfield::ppcbit<S, bool, 61> TLB0_FI;
	bitfield::ppcbit<S, bool, 62> TLB1_FI;
};

// MMUCFG	1015
#endif

/*
 * ATB - Alternate Time Base
 */
#if defined(POWER_CAT_ATB)
// ATBL		526
// ATBU		527
#endif

/*
 * E.CD - Embedded.Cache Debug
 */
#if defined(POWER_CAT_E_CD)
// DCDBTRL	924
// DCDBTRH	925
// ICDBTRL	926
// ICDBTRH	927
// ICDBDR	979
#endif

/*
 * E.ED - Embedded.Enhanced Debug
 */
#if defined(POWER_CAT_E_ED)
// DSRR0	574
// DSRR1	575
// SPRG9	605
#endif

/*
 * E.PD - Embedded.External PID
 */
#if defined(POWER_CAT_E_PD)
// EPLC		947
// EPSC		948
#endif

/*
 * E.HV - Embedded.Hypervisor
 */
#if defined(POWER_CAT_E_HV)
// GDECAR	53
#if defined(POWER_CAT_E_PT)
// LPER		56
// LPERU	57
#endif
// GTSRWR	60
// DBSRWR	306
// MSRP		311
// LPIDR	338
// MAS5		339
// MAS8		341
// TLB0PS	344
// TLB1PS	345
// TLB2PS	346
// TLB3PS	347
#if defined(POWER_CAT_64)
// MAS5_MAS6	348
// MAS8_MAS1	349
#endif
// GSPRG0	368
// GSPRG1	369
// GSPRG2	370
// GSPRG3	371
// GDEC		374
// GTCR		375
// GTSR		376
// GSRR0	378
// GSRR1	379
#if defined(POWER_CAT_EXP)
// GEPR		380
#endif
// GDEAR	381
// GPIR		382
// GESR		383
// IVOR38	432
// IVOR39	433
// IVOR40	434
// IVOR41	435
// GIVOR2	440
// GIVOR3	441
// GIVOR4	442
// GIVOR8	443
// GIVOR13	444
// GIVOR14	445
#if defined(POWER_CAT_E_PM)
// GIVOR35	464
#endif
// GIVPR	447
// GIVOR10	474
// GIVOR11	475
// GIVOR12	476
#endif

#if defined(POWER_CAT_E_HV) || (defined(POWER_CAT_E) && defined(POWER_CAT_64))
// EPCR		307
#endif

/*
 * E.HV.LRAT - Embedded.Hypervisor.LRAT
 */
#if defined(POWER_CAT_E_HV_LRAT)
// LRATCFG	342
// LRATPS	343
// IVOR42	436
#endif

/*
 * E.MT - Embedded.Multi-Threading
 */
#if defined(POWER_CAT_E_MT)
// TENSR	437
// TENS		438
// TENC		439
// TIR		446
#endif

/*
 * E.PC - Embedded.Processor Control
 */
#if defined(POWER_CAT_PC)
// IVOR36	532
// IVOR37	533
#endif

/*
 * E.PM - Embedded.Performance Monitor
 */
#if defined(POWER_CAT_E_PM)
// IVOR35	531
#endif

/*
 * E.PT - Embedded.Page Table
 */
#if defined(POWER_CAT_E_PT)
// EPTCFG	350
#endif

/*
 * EC - External Control
 */
#if defined(POWER_CAT_EC)
// EAR		282
#endif

/*
 * EXP - External Proxy
 */
#if defined(POWER_CAT_EXP)
// EPR		702
#endif

/*
 * SP - Signal Processing
 */
#if defined(POWER_CAT_SP)
union SPEFSCR {
	static constexpr auto SPRN = 512;
	using S = uint32_t;
	struct { S r; };
	bitfield::ppcbit<S, bool, 32> SOVH;
	bitfield::ppcbit<S, bool, 33> OVH;
	bitfield::ppcbit<S, bool, 34> FGH;
	bitfield::ppcbit<S, bool, 35> FXH;
	bitfield::ppcbit<S, bool, 36> FINVH;
	bitfield::ppcbit<S, bool, 37> FDBZH;
	bitfield::ppcbit<S, bool, 38> FUNFH;
	bitfield::ppcbit<S, bool, 39> FOVFH;
	bitfield::ppcbit<S, bool, 42> FINXS;
	bitfield::ppcbit<S, bool, 43> FINVS;
	bitfield::ppcbit<S, bool, 44> FDBZS;
	bitfield::ppcbit<S, bool, 45> FUNFS;
	bitfield::ppcbit<S, bool, 46> FOVFS;
	bitfield::ppcbit<S, bool, 48> SOV;
	bitfield::ppcbit<S, bool, 49> OV;
	bitfield::ppcbit<S, bool, 50> FG;
	bitfield::ppcbit<S, bool, 51> FX;
	bitfield::ppcbit<S, bool, 52> FINV;
	bitfield::ppcbit<S, bool, 53> FDBZ;
	bitfield::ppcbit<S, bool, 54> FUNF;
	bitfield::ppcbit<S, bool, 55> FOVF;
	bitfield::ppcbit<S, bool, 57> FINXE;
	bitfield::ppcbit<S, bool, 58> FINVE;
	bitfield::ppcbit<S, bool, 59> FDBZE;
	bitfield::ppcbit<S, bool, 60> FUNFE;
	bitfield::ppcbit<S, bool, 61> FOVFE;
	enum class RoundingMode {
		Nearest = 0,
		Zero = 1,
		Positive_Infinity = 2,
		Negative_Infinity = 3,
	};
	bitfield::ppcbits<S, RoundingMode, 62, 63> FRMC;

};

/* Interrupt Vector Offset Registers */
union IVOR32 {
	static constexpr auto SPRN = 528;
	uint32_t r;
};

union IVOR33 {
	static constexpr auto SPRN = 529;
	uint32_t r;
};

union IVOR34 {
	static constexpr auto SPRN = 530;
	uint32_t r;
};
#endif

/*
 * STM - Stream
 */
#if defined(POWER_CAT_STM)
// DSCR_UNPRIV	3
// DSCR		17
#endif

/*
 * TM - Transactional Memory
 */
#if defined(POWER_CAT_TM)
// TFHAR	128
// TFIAR	129
// TEXASR	130
// TEXASRU	131
#endif
