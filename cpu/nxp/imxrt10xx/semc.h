#pragma once

#include <cassert>
#include <cstdint>

/*
 * Smart External Memory Controller
 */
union semc_br {
	struct {
		uint32_t VLD : 1;
		uint32_t MS : 5;
		uint32_t : 6;
		uint32_t BA : 20;
	};
	uint32_t r;
};

enum semc_mcr_dqsmd {
	DQSMD_internal,
	DQSMD_from_pad,
};

enum semc_ipcmd_cmd {
	CMD_SDRAM_READ = 0x8,
	CMD_SDRAM_WRITE = 0x9,
	CMD_SDRAM_MODESET = 0xa,
	CMD_SDRAM_ACTIVE = 0xb,
	CMD_SDRAM_AUTO_REFRESH = 0xc,
	CMD_SDRAM_SELF_REFRESH = 0xd,
	CMD_SDRAM_PRECHARGE = 0xe,
	CMD_SDRAM_PRECHARGE_ALL = 0xf
};

struct semc {
	union semc_mcr {
		struct {
			uint32_t SWRST : 1;
			uint32_t MDIS : 1;
			enum semc_mcr_dqsmd DQSMD : 1;
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
	union semc_iocr {
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
	union semc_bmcr0 {
		struct {
			uint32_t WQOS : 4;
			uint32_t WAGE : 4;
			uint32_t WSH : 8;
			uint32_t WRWS : 8;
			uint32_t : 8;
		};
		uint32_t r;
	} BMCR0;
	union semc_bmcr1 {
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
		union semc_br BR[9];
		struct {
			union semc_br BR0;
			union semc_br BR1;
			union semc_br BR2;
			union semc_br BR3;
			union semc_br BR4;
			union semc_br BR5;
			union semc_br BR6;
			union semc_br BR7;
			union semc_br BR8;
		};
	};
	uint32_t : 32;
	uint32_t INTEN;
	union semc_intr {
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
	union semc_sdramcr0 {
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
	union semc_sdramcr1 {
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
	union semc_sdramcr2 {
		struct {
			uint32_t SRRC : 8;
			uint32_t REF2REF : 8;
			uint32_t ACT2ACT : 8;
			uint32_t ITO : 8;
		};
		uint32_t r;
	} SDRAMCR2;
	union semc_sdramcr3 {
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
	union semc_ipcr0 {
		struct {
			uint32_t SA : 32;
		};
		uint32_t r;
	} IPCR0;
	union semc_ipcr1 {
		struct {
			uint32_t DATSZ : 3;
			uint32_t : 29;
		};
		uint32_t r;
	} IPCR1;
	union semc_ipcr2 {
		struct {
			uint32_t BM0 : 1;
			uint32_t BM1 : 1;
			uint32_t BM2 : 1;
			uint32_t BM3 : 1;
			uint32_t : 28;
		};
		uint32_t r;
	} IPCR2;
	union semc_ipcmd {
		struct {
			enum semc_ipcmd_cmd CMD : 16;
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
static_assert(sizeof(struct semc) == 0x100, "");

#define SEMC_ADDR 0x402f0000
static struct semc *const SEMC = (struct semc*)SEMC_ADDR;
