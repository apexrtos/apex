#pragma once

#include <assert.h>
#include <stdint.h>

#define FLEXSPI_OPCODE_CMD_SDR 0x01
#define FLEXSPI_OPCODE_CMD_DDR 0x21
#define FLEXSPI_OPCODE_RADDR_SDR 0x02
#define FLEXSPI_OPCODE_RADDR_DDR 0x22
#define FLEXSPI_OPCODE_CADDR_SDR 0x03
#define FLEXSPI_OPCODE_CADDR_DDR 0x23
#define FLEXSPI_OPCODE_MODE1_SDR 0x04
#define FLEXSPI_OPCODE_MODE1_DDR 0x24
#define FLEXSPI_OPCODE_MODE2_SDR 0x05
#define FLEXSPI_OPCODE_MODE2_DDR 0x25
#define FLEXSPI_OPCODE_MODE4_SDR 0x06
#define FLEXSPI_OPCODE_MODE4_DDR 0x26
#define FLEXSPI_OPCODE_MODE8_SDR 0x07
#define FLEXSPI_OPCODE_MODE8_DDR 0x27
#define FLEXSPI_OPCODE_WRITE_SDR 0x08
#define FLEXSPI_OPCODE_WRITE_DDR 0x28
#define FLEXSPI_OPCODE_READ_SDR 0x09
#define FLEXSPI_OPCODE_READ_DDR 0x29
#define FLEXSPI_OPCODE_LEARN_SDR 0x0A
#define FLEXSPI_OPCODE_LEARN_DDR 0x2A
#define FLEXSPI_OPCODE_DATSZ_SDR 0x0B
#define FLEXSPI_OPCODE_DATSZ_DDR 0x2B
#define FLEXSPI_OPCODE_DUMMY_SDR 0x0C
#define FLEXSPI_OPCODE_DUMMY_DDR 0x2C
#define FLEXSPI_OPCODE_DUMMY_RWDS_SDR 0x0D
#define FLEXSPI_OPCODE_DUMMY_RWDS_DDR 0x2D
#define FLEXSPI_OPCODE_JMP_ON_CS 0x1F
#define FLEXSPI_OPCODE_STOP 0

#define FLEXSPI_NUM_PADS_1 0
#define FLEXSPI_NUM_PADS_2 1
#define FLEXSPI_NUM_PADS_4 2
#define FLEXSPI_NUM_PADS_8 3

struct flexspi_lut_entry {
	uint32_t operand0 : 8;
	uint32_t num_pads0 : 2;
	uint32_t opcode0 : 6;
	uint32_t operand1 : 8;
	uint32_t num_pads1 : 2;
	uint32_t opcode1 : 6;
};

struct flexspi_lut_seq {
	uint8_t num;
	uint8_t id;
	uint16_t : 16;
};

/*
 * FlexSPI Configuration block
 *
 * REVISIT: EVKB-IMXRT1050 uses features not in reference manual rev 1.
 */
struct flexspi_boot_config {
	char tag[4];
	struct {
		uint8_t bugfix;
		uint8_t minor;
		uint8_t major;
		uint8_t ascii;
	} version;
	uint32_t : 32;
	uint8_t readSampleClkSrc;
	uint8_t csHoldTime;
	uint8_t csSetupTime;
	uint8_t columnAddressWidth;
	uint8_t deviceModeCfgEnable;
	uint8_t deviceModeType;		/* REVISIT: undocumented */
	uint16_t waitTimeCfgCommands;
	struct flexspi_lut_seq deviceModeSeq;
	uint32_t deviceModeArg;
	uint8_t configCmdEnable;
	uint8_t configModeType[3];	/* REVISIT: undocumented */
	struct flexspi_lut_seq configCmdSeqs[3];
	uint32_t : 32;
	uint32_t cfgCmdArgs[3];
	uint32_t : 32;
	uint32_t controllerMiscOption;
	uint8_t deviceType;
	uint8_t sflashPadType;
	uint8_t serialClkFreq;
	uint8_t lutCustomSeqEnable;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t sflashA1Size;
	uint32_t sflashA2Size;
	uint32_t sflashB1Size;
	uint32_t sflashB2Size;
	uint32_t csPadSettingOverride;
	uint32_t sclkPadSettingOverride;
	uint32_t dataPadSettingOverride;
	uint32_t dqsPadSettingOverride;
	uint32_t timeoutInMs;
	uint32_t commandInterval;
	uint16_t dataValidTime_DLLA;
	uint16_t dataValidTime_DLLB;
	uint16_t busyOffset;
	uint16_t busyBitPolarity;
	struct flexspi_lut_entry lookupTable[64];
	struct flexspi_lut_seq lutCustomSeq[12];
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
};
static_assert(sizeof(struct flexspi_boot_config) == 448);

/*
 * Table 8-17. Serial NOR configuration block
 *
 * REVISIT: EVKB-IMXRT1050 uses features not in reference manual rev 1.
 */
struct flexspi_boot_nor {
	struct flexspi_boot_config memConfig;
	uint32_t pageSize;
	uint32_t sectorSize;
	uint8_t ipCmdSerialClkFreq;
	uint8_t isUniformBlockSize;     /* REVISIT: undocumented */
	uint8_t isDataOrderSwapped;	/* REVISIT: undocumented */
	uint8_t : 8;
	uint8_t serialNorType;          /* REVISIT: undocuemnted */
	uint8_t needExitNoCmdMode;      /* REVISIT: undocumented */
	uint8_t halfClkForNonReadCmd;   /* REVISIT: undocumented */
	uint8_t needRestoreNoCmdMode;   /* REVISIT: undocumented */
	uint32_t blockSize;             /* REVISIT: undocuemnted */
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
};
static_assert(sizeof(struct flexspi_boot_nor) == 512);
