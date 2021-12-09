#pragma once

/*
 * See The RISC-V Instruction Set Manual Volume II: Privileged Architecture
 */

#include <conf/config.h>
#include <cstdint>
#include <lib/bitfield.h>

/* Machine Status Register */
union mstatus {
	static constexpr auto CSRN = 0x300;
	using S = uint32_t;
	struct { S r; };
	bitfield::bit<S, bool, 31> SD;
	bitfield::bit<S, bool, 22> TSR;
	bitfield::bit<S, bool, 21> TW;
	bitfield::bit<S, bool, 20> TVM;
	bitfield::bit<S, bool, 19> MXR;
	bitfield::bit<S, bool, 18> SUM;
	bitfield::bit<S, bool, 17> MPRV;
	enum class ExtensionState {
		All_off = 0,
		Some_on = 1,
		Some_clean = 2,
		Some_dirty = 3,
	};
	bitfield::bits<S, ExtensionState, 15, 2> XS;
	enum class FPUState {
		Off = 0,
		Initial = 1,
		Clean = 2,
		Dirty = 3,
	};
	bitfield::bits<S, FPUState, 13, 2> FS;
	bitfield::bits<S, unsigned, 11, 2> MPP;
	bitfield::bit<S, bool, 8> SPP;
	bitfield::bit<S, bool, 7> MPIE;
	bitfield::bit<S, bool, 5> SPIE;
	bitfield::bit<S, bool, 4> UPIE;
	bitfield::bit<S, bool, 3> MIE;
	bitfield::bit<S, bool, 1> SIE;
	bitfield::bit<S, bool, 0> UIE;
};

/* Machine Interrupt Enable Register */
union mie {
	static constexpr auto CSRN = 0x304;
	using S = uint32_t;
	struct { S r; };
	bitfield::bit<S, bool, 11> MEIE;    /* Machine External Interrupt Enable */
	bitfield::bit<S, bool, 9> SEIE;	    /* Supervisor External Interrupt Enable */
	bitfield::bit<S, bool, 8> UEIE;	    /* User External Interrupt Enable */
	bitfield::bit<S, bool, 7> MTIE;	    /* Machine Timer Interrupt Enable */
	bitfield::bit<S, bool, 5> STIE;	    /* Supervisor Timer Interrupt Enable */
	bitfield::bit<S, bool, 4> UTIE;	    /* User Timer Interrupt Enable */
	bitfield::bit<S, bool, 3> MSIE;	    /* Machine Software Interrupt Enable */
	bitfield::bit<S, bool, 1> SSIE;	    /* Supervisor Software Interrupt Enable */
	bitfield::bit<S, bool, 0> USIE;	    /* User Software Interrupt Enable */
};

/* Machine Scratch Register */
union mscratch {
	static constexpr auto CSRN = 0x340;
	uint32_t r;
};

/* Machine Exception Program Counter */
union mepc {
	static constexpr auto CSRN = 0x341;
	uint32_t r;
};

/* Machine Trap Cause */
union mcause {
	static constexpr auto CSRN = 0x342;
	uint32_t r;
};

/* Machine Trap Value */
union mtval {
	static constexpr auto CSRN = 0x343;
	uint32_t r;
};

#ifdef CONFIG_MPU
union pmpcfg {
	using S = uint8_t;
	struct { S r; };
	pmpcfg() : r{} {}
	pmpcfg(S s) : r{s} {}
	bitfield::bit<S, bool, 7> L;	    /* lock entry and apply to M mode */
	enum class Match {
		OFF = 0,    /* region disabled */
		TOR = 1,    /* top of range */
		NA4 = 2,    /* naturally aligned 4-byte region */
		NAPOT = 3,  /* naturally aligned power-of-2 region, >= 8 byte */
	};
	bitfield::bits<S, Match, 3, 2> A;   /* address matching mode */
	bitfield::bit<S, bool, 2> X;
	bitfield::bit<S, bool, 1> W;
	bitfield::bit<S, bool, 0> R;
	constexpr
	operator S() const
	{
		return r;
	}
};

/* Physical Memory Protection Configuration Registers */
union pmpcfg0 {
	static constexpr auto CSRN = 0x3a0;
	using S = uint32_t;
	struct { S r; };
	bitfield::bits<S, pmpcfg, 0, 8> pmp0cfg;
	bitfield::bits<S, pmpcfg, 8, 8> pmp1cfg;
	bitfield::bits<S, pmpcfg, 16, 8> pmp2cfg;
	bitfield::bits<S, pmpcfg, 24, 8> pmp3cfg;
};

union pmpcfg1 {
	static constexpr auto CSRN = 0x3a1;
	using S = uint32_t;
	struct { S r; };
	bitfield::bits<S, pmpcfg, 0, 8> pmp4cfg;
	bitfield::bits<S, pmpcfg, 8, 8> pmp5cfg;
	bitfield::bits<S, pmpcfg, 16, 8> pmp6cfg;
	bitfield::bits<S, pmpcfg, 24, 8> pmp7cfg;
};

union pmpcfg2 {
	static constexpr auto CSRN = 0x3a2;
	using S = uint32_t;
	struct { S r; };
	bitfield::bits<S, pmpcfg, 0, 8> pmp8cfg;
	bitfield::bits<S, pmpcfg, 8, 8> pmp9cfg;
	bitfield::bits<S, pmpcfg, 16, 8> pmp10cfg;
	bitfield::bits<S, pmpcfg, 24, 8> pmp11cfg;
};

union pmpcfg3 {
	static constexpr auto CSRN = 0x3a3;
	using S = uint32_t;
	struct { S r; };
	bitfield::bits<S, pmpcfg, 0, 8> pmp12cfg;
	bitfield::bits<S, pmpcfg, 8, 8> pmp13cfg;
	bitfield::bits<S, pmpcfg, 16, 8> pmp14cfg;
	bitfield::bits<S, pmpcfg, 24, 8> pmp15cfg;
};

/* Physical Memory Protection Address Registers */
union pmpaddr0 {
	static constexpr auto CSRN = 0x3b0;
	uint32_t r;
};

union pmpaddr1 {
	static constexpr auto CSRN = 0x3b1;
	uint32_t r;
};

union pmpaddr2 {
	static constexpr auto CSRN = 0x3b2;
	uint32_t r;
};

union pmpaddr3 {
	static constexpr auto CSRN = 0x3b3;
	uint32_t r;
};

union pmpaddr4 {
	static constexpr auto CSRN = 0x3b4;
	uint32_t r;
};

union pmpaddr5 {
	static constexpr auto CSRN = 0x3b5;
	uint32_t r;
};

union pmpaddr6 {
	static constexpr auto CSRN = 0x3b6;
	uint32_t r;
};

union pmpaddr7 {
	static constexpr auto CSRN = 0x3b7;
	uint32_t r;
};

union pmpaddr8 {
	static constexpr auto CSRN = 0x3b8;
	uint32_t r;
};

union pmpaddr9 {
	static constexpr auto CSRN = 0x3b9;
	uint32_t r;
};

union pmpaddr10 {
	static constexpr auto CSRN = 0x3ba;
	uint32_t r;
};

union pmpaddr11 {
	static constexpr auto CSRN = 0x3bb;
	uint32_t r;
};

union pmpaddr12 {
	static constexpr auto CSRN = 0x3bc;
	uint32_t r;
};

union pmpaddr13 {
	static constexpr auto CSRN = 0x3bd;
	uint32_t r;
};

union pmpaddr14 {
	static constexpr auto CSRN = 0x3be;
	uint32_t r;
};

union pmpaddr15 {
	static constexpr auto CSRN = 0x3bf;
	uint32_t r;
};

#endif
