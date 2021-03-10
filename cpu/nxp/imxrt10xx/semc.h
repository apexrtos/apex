#pragma once

#include <cassert>
#include <cstdint>

/*
 * Smart External Memory Controller
 */
struct semc {
	union br {
		struct {
			uint32_t VLD : 1;
			uint32_t MS : 5;
			uint32_t : 6;
			uint32_t BA : 20;
		};
		uint32_t r;
	};

	union mcr {
		enum class dqsmd : unsigned {
			INTERNAL,
			FROM_PAD,
		};

		struct {
			uint32_t SWRST : 1;
			uint32_t MDIS : 1;
			dqsmd DQSMD : 1;
			uint32_t : 3;
			uint32_t WPOL0 : 1;
			uint32_t WPOL1 : 1;
			uint32_t : 8;
			uint32_t CTO : 8;
			uint32_t BTO : 5;
			uint32_t : 3;
		};
		uint32_t r;
	} MCR;
	union iocr {
		struct {
			uint32_t MUX_A8 : 3;
			uint32_t MUX_CSX0 : 3;
			uint32_t MUX_CSX1 : 3;
			uint32_t MUX_CSX2 : 3;
			uint32_t MUX_CSX3 : 3;
			uint32_t MUX_RDY : 3;
			uint32_t : 14;
		};
		uint32_t r;
	} IOCR;
	union bmcr0 {
		struct {
			uint32_t WQOS : 4;
			uint32_t WAGE : 4;
			uint32_t WSH : 8;
			uint32_t WRWS : 8;
			uint32_t : 8;
		};
		uint32_t r;
	} BMCR0;
	union bmcr1 {
		struct {
			uint32_t WQOS : 4;
			uint32_t WAGE : 4;
			uint32_t WPH : 8;
			uint32_t WRWS : 8;
			uint32_t WBR : 8;
		};
		uint32_t r;
	} BMCR1;
	union {
		br BR[9];
		struct {
			br BR0;
			br BR1;
			br BR2;
			br BR3;
			br BR4;
			br BR5;
			br BR6;
			br BR7;
			br BR8;
		};
	};
	uint32_t : 32;
	uint32_t INTEN;
	union intr {
		struct {
			uint32_t IPCMDDONE : 1;
			uint32_t IPCMDERR : 1;
			uint32_t AXICMDERR : 1;
			uint32_t AXIBUSERR : 1;
			uint32_t NDPAGEEND : 1;
			uint32_t NDNOPEND : 1;
			uint32_t : 26;
		};
		uint32_t r;
	} INTR;
	union sdramcr0 {
		struct {
			uint32_t PS : 1;
			uint32_t : 3;
			uint32_t BL : 3;
			uint32_t COL8 : 1; /* For IMXRT106x only. RSV for IMXRT105x */
			uint32_t COL : 2;
			uint32_t CL : 2;
			uint32_t : 12;
		};
		uint32_t r;
	} SDRAMCR0;
	union sdramcr1 {
		struct {
			uint32_t PRE2ACT : 4;
			uint32_t ACT2RW : 4;
			uint32_t RFRC : 5;
			uint32_t WRC : 3;
			uint32_t CKEOFF : 4;
			uint32_t ACT2PRE : 4;
			uint32_t : 8;
		};
		uint32_t r;
	} SDRAMCR1;
	union sdramcr2 {
		struct {
			uint32_t SRRC : 8;
			uint32_t REF2REF : 8;
			uint32_t ACT2ACT : 8;
			uint32_t ITO : 8;
		};
		uint32_t r;
	} SDRAMCR2;
	union sdramcr3 {
		struct {
			uint32_t REN : 1;
			uint32_t REBL : 3;
			uint32_t : 4;
			uint32_t PRESCALE : 8;
			uint32_t RT : 8;
			uint32_t UT : 8;
		};
		uint32_t r;
	} SDRAMCR3;
	uint32_t NANDCR0;
	uint32_t NANDCR1;
	uint32_t NANDCR2;
	uint32_t NANDCR3;
	uint32_t NORCR0;
	uint32_t NORCR1;
	uint32_t NORCR2;
	uint32_t NORCR3;
	uint32_t SRAMCR0;
	uint32_t SRAMCR1;
	uint32_t SRAMCR2;
	uint32_t SRAMCR3;
	uint32_t DBICR0;
	uint32_t DBICR1;
	uint32_t : 32;
	uint32_t : 32;
	union ipcr0 {
		struct {
			uint32_t SA : 32;
		};
		uint32_t r;
	} IPCR0;
	union ipcr1 {
		struct {
			uint32_t DATSZ : 3;
			uint32_t : 29;
		};
		uint32_t r;
	} IPCR1;
	union ipcr2 {
		struct {
			uint32_t BM0 : 1;
			uint32_t BM1 : 1;
			uint32_t BM2 : 1;
			uint32_t BM3 : 1;
			uint32_t : 28;
		};
		uint32_t r;
	} IPCR2;
	union ipcmd {
		enum class cmd : unsigned {
			READ = 0x8,
			WRITE = 0x9,
			MODESET = 0xa,
			ACTIVE = 0xb,
			AUTO_REFRESH = 0xc,
			SELF_REFRESH = 0xd,
			PRECHARGE = 0xe,
			PRECHARGE_ALL = 0xf
		};

		struct {
			cmd CMD : 16;
			uint32_t KEY : 16;
		};
		uint32_t r;
	} IPCMD;
	uint32_t IPTXDAT;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t IPRXDAT;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t STS0;
	uint32_t STS1;
	uint32_t STS2;
	uint32_t STS3;
	uint32_t STS4;
	uint32_t STS5;
	uint32_t STS6;
	uint32_t STS7;
	uint32_t STS8;
	uint32_t STS9;
	uint32_t STS10;
	uint32_t STS11;
	uint32_t STS12;
	uint32_t STS13;
	uint32_t STS14;
	uint32_t STS15;
};
static_assert(sizeof(semc) == 0x100);

#define SEMC_ADDR 0x402f0000
static semc *const SEMC = (semc*)SEMC_ADDR;
