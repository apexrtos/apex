#include <cpu/nxp/imxrt10xx/image/dcd.h>

#include <cpu/nxp/imxrt10xx/ccm.h>
#include <cpu/nxp/imxrt10xx/iomuxc.h>
#include <cpu/nxp/imxrt10xx/semc.h>
#include <cstddef>

/*
 * REVISIT: Rewrite using std::bit_cast (c++20)
 */
constexpr uint32_t bswap(uint32_t v)
{
	return __builtin_bswap32(v);
}

constexpr uint32_t bswap(ccm::ccm_cbcdr v)
{
	uint32_t tmp =
	    v.SEMC_CLK_SEL << 6 |
	    v.SEMC_ALT_CLK_SEL << 7 |
	    v.IPG_PODF << 8 |
	    v.AHB_PODF << 10 |
	    v.SEMC_PODF << 16 |
	    v.PERIPH_CLK_SEL << 25 |
	    v.PERIPH_CLK2_PODF << 27;
	return bswap(tmp);
}

constexpr uint32_t bswap(iomuxc_sw_mux_ctl v)
{
	uint32_t tmp =
		v.MUX_MODE << 0 |
		v.SION << 4;
	return bswap(tmp);
}

constexpr uint32_t bswap(iomuxc_sw_pad_ctl v)
{
	uint32_t tmp =
	    v.SRE << 0 |
	    v.DSE << 3 |
	    v.SPEED << 6 |
	    v.ODE << 11 |
	    v.PKE << 12 |
	    v.PUE << 13 |
	    v.PUS << 14 |
	    v.HYS << 16;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_mcr v)
{
	uint32_t tmp =
	    v.SWRST << 0 |
	    v.MDIS << 1 |
	    v.DQSMD << 2 |
	    v.WPOL0 << 6 |
	    v.WPOL1 << 7 |
	    v.CTO << 16 |
	    v.BTO << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_bmcr0 v)
{
	uint32_t tmp =
	    v.WQOS << 0 |
	    v.WAGE << 4 |
	    v.WSH << 8 |
	    v.WRWS << 16;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_bmcr1 v)
{
	uint32_t tmp =
	    v.WQOS << 0 |
	    v.WAGE << 4 |
	    v.WPH << 8 |
	    v.WRWS << 16 |
	    v.WBR << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_iocr v)
{
	uint32_t tmp =
	    v.MUX_A8 << 0 |
	    v.MUX_CSX0 << 3 |
	    v.MUX_CSX1 << 6 |
	    v.MUX_CSX2 << 9 |
	    v.MUX_CSX3 << 12 |
	    v.MUX_RDY << 15;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc_br v)
{
	uint32_t tmp =
	    v.VLD << 0 |
	    v.MS << 1 |
	    v.BA << 12;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_sdramcr0 v)
{
	uint32_t tmp =
	    v.PS << 0 |
	    v.BL << 4 |
	    v.COL << 8 |
	    v.CL << 10;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_sdramcr1 v)
{
	uint32_t tmp =
	    v.PRE2ACT << 0 |
	    v.ACT2RW << 4 |
	    v.RFRC << 8 |
	    v.WRC << 13 |
	    v.CKEOFF << 16 |
	    v.ACT2PRE << 20;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_sdramcr2 v)
{
	uint32_t tmp =
	    v.SRRC << 0 |
	    v.REF2REF << 8 |
	    v.ACT2ACT << 16 |
	    v.ITO << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_sdramcr3 v)
{
	uint32_t tmp =
	    v.REN << 0 |
	    v.REBL << 1 |
	    v.PRESCALE << 8 |
	    v.RT << 16 |
	    v.UT << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_ipcr0 v)
{
	return bswap(v.SA);
}

constexpr uint32_t bswap(semc::semc_ipcr1 v)
{
	return bswap(v.DATSZ);
}

constexpr uint32_t bswap(semc::semc_ipcr2 v)
{
	uint32_t tmp =
	    v.BM0 << 0 |
	    v.BM1 << 1 |
	    v.BM2 << 2 |
	    v.BM3 << 3;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_ipcmd v)
{
	uint32_t tmp =
	    v.CMD << 0 |
	    v.KEY << 16;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::semc_intr v)
{
	uint32_t tmp =
	    v.IPCMDDONE << 0 |
	    v.IPCMDERR << 1 |
	    v.AXICMDERR << 2 |
	    v.AXIBUSERR << 3 |
	    v.NDPAGEEND << 4 |
	    v.NDNOPEND << 5;
	return bswap(tmp);
}

/* Configuration for SEMC (SDRAM) pad */
constexpr uint32_t semc_pad_control = bswap((iomuxc_sw_pad_ctl){{
	.SRE = SRE_Fast,
	.DSE = DSE_R0_7,
	.SPEED = SPEED_200MHz,
	.ODE = ODE_Open_Drain_Disabled,
	.PKE = PKE_Pull_Keeper_Enabled,
	.PUE = PUE_Keeper,
	.PUS = PUS_100K_Pull_Down,
	.HYS = HYS_Hysteresis_Enabled,
}});

constexpr struct dcd {
	dcd_header hdr = {
		.tag = DCD_HEADER_TAG,
		.length_be = __builtin_bswap16(sizeof(dcd)),
		.version = DCD_HEADER_VERSION,
	};

	struct {
		dcd_command cmd = {
			.tag = DCD_WRITE_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd1)),
			.parameter = DCD_WRITE_PARAM_WRITE32,
		};
		uint32_t args[196] = {
			/* Set SEMC clock source to PLL2.PFD2/3 = 166MHz. */
			bswap(CCM_ADDR + offsetof(ccm, CBCDR)),
			bswap((ccm::ccm_cbcdr){{
				.SEMC_CLK_SEL = SEMC_CLK_SEL_ALTERNATE,
				.SEMC_ALT_CLK_SEL = SEMC_ALT_CLK_SEL_PLL2_PFD2,
				.IPG_PODF = 3, /* divide by 4 */
				.AHB_PODF = 0, /* divide by 1 */
				.SEMC_PODF = 2, /* divide by 3 */
				.PERIPH_CLK_SEL = PERIPH_CLK_SEL_PRE_PERIPH,
				.PERIPH_CLK2_PODF = 0, /* divide by 1 */
			}}),

			/* Ungate SEMC clock */
			bswap(CCM_ADDR + offsetof(ccm, CCGR3)),
			bswap(0xf00ff330),

			/* Configure SEMC pad multiplexing. */
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_00)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_01)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_02)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_03)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_04)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_05)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_06)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_07)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_08)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_09)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_10)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_11)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_12)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_13)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_14)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_15)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_16)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_17)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_18)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_19)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_20)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_21)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_22)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_23)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_24)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_25)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_26)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_27)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_28)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_29)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_30)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_31)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_32)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_33)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_34)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_35)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_36)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_37)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_38)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_39)),
			bswap((iomuxc_sw_mux_ctl){{
				.MUX_MODE = 0,
				.SION = SION_Software_Input_On_Enabled,
			}}),
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_40)), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC_41)), 0,

			/* Configure SEMC pad control registers */
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_00)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_01)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_02)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_03)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_04)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_05)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_06)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_07)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_08)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_09)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_10)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_11)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_12)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_13)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_14)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_15)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_16)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_17)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_18)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_19)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_20)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_21)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_22)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_23)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_24)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_25)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_26)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_27)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_28)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_29)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_30)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_31)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_32)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_33)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_34)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_35)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_36)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_37)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_38)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_39)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_40)), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC_41)), semc_pad_control,

			/* Configure DRAM on SEMC module. */
			bswap(SEMC_ADDR + offsetof(semc, MCR)),
			bswap((semc::semc_mcr){{
				.SWRST = 0,
				.MDIS = 0,
				.DQSMD = DQSMD_from_pad,
				.WPOL0 = 0,
				.WPOL1 = 0,
				.CTO = 0,
				.BTO = 16,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, IOCR)),
			bswap((semc::semc_iocr){{
				.MUX_A8 = 0,	/* SDRAM Address bit (A8) */
				.MUX_CSX0 = 5,	/* NOR CE# */
				.MUX_CSX1 = 6,	/* PSRAM CE# */
				.MUX_CSX2 = 4,	/* NAND CE# */
				.MUX_CSX3 = 7,	/* DBI CSX */
				.MUX_RDY = 0,	/* NAND Ready/Wait# input */
			}}),
			bswap(SEMC_ADDR + offsetof(semc, BMCR0)),
			bswap((semc::semc_bmcr0){{
				.WQOS = 4,
				.WAGE = 2,
				.WSH = 5,
				.WRWS = 3,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, BMCR1)),
			bswap((semc::semc_bmcr1){{
				.WQOS = 4,
				.WAGE = 2,
				.WPH = 5,
				.WRWS = 3,
				.WBR = 6,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, BR0)),
			bswap((semc_br){{
				.VLD = 1,
				.MS = 13,	/* 32MiB */
				.BA = 0x80000,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR0)),
			bswap((semc::semc_sdramcr0){{
				.PS = 1,	/* 16 bit */
				.BL = 3,	/* burst length 8 */
				.COL = 3,	/* 9 bit columns */
				.CL = 3,	/* CAS latency 3 */
			}}),
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR1)),
			bswap((semc::semc_sdramcr1){{
				.PRE2ACT = 2,
				.ACT2RW = 2,
				.RFRC = 9,
				.WRC = 1,
				.CKEOFF = 0,
				.ACT2PRE = 6,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR2)),
			bswap((semc::semc_sdramcr2){{
				.SRRC = 10,
				.REF2REF = 9,
				.ACT2ACT = 9,
				.ITO = 0,
			}}),

			/* Prepare to send DRAM commands. */
			bswap(SEMC_ADDR + offsetof(semc, IPCR0)),
			bswap((semc::semc_ipcr0){{
				.SA = 0x80000000,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, IPCR1)),
			bswap((semc::semc_ipcr1){{
				.DATSZ = 2,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, IPCR2)),
			bswap((semc::semc_ipcr2){{
				.BM0 = 0,
				.BM1 = 0,
				.BM2 = 0,
				.BM3 = 0,
			}}),

			/* Issue precharge all command. */
			bswap(SEMC_ADDR + offsetof(semc, IPCMD)),
			bswap((semc::semc_ipcmd){{
				.CMD = CMD_SDRAM_PRECHARGE_ALL,
				.KEY = 0xa55a,
			}}),
		};
	} cmd1;

	/* Wait for precharge all command to complete. */
	struct {
		dcd_command cmd = {
			.tag = DCD_CHECK_DATA_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd2)),
			.parameter = DCD_CHECK_DATA_PARAM_ANY_SET32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, INTR)),
			bswap((semc::semc_intr){{
				.IPCMDDONE = 1,
			}}),
		};
	} cmd2;

	/* Issue auto refresh command. */
	struct {
		dcd_command cmd = {
			.tag = DCD_WRITE_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd3)),
			.parameter = DCD_WRITE_PARAM_WRITE32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, IPCMD)),
			bswap((semc::semc_ipcmd){{
				.CMD = CMD_SDRAM_AUTO_REFRESH,
				.KEY = 0xa55a,
			}}),
		};
	} cmd3;

	/* Wait for auto refresh command to complete. */
	struct {
		dcd_command cmd = {
			.tag = DCD_CHECK_DATA_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd4)),
			.parameter = DCD_CHECK_DATA_PARAM_ANY_SET32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, INTR)),
			bswap((semc::semc_intr){{
				.IPCMDDONE = 1,
			}}),
		};
	} cmd4;

	/* Issue auto refresh command. */
	struct {
		dcd_command cmd = {
			.tag = DCD_WRITE_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd5)),
			.parameter = DCD_WRITE_PARAM_WRITE32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, IPCMD)),
			bswap((semc::semc_ipcmd){{
				.CMD = CMD_SDRAM_AUTO_REFRESH,
				.KEY = 0xa55a,
			}}),
		};
	} cmd5;

	/* Wait for auto refresh command to complete. */
	struct {
		dcd_command cmd = {
			.tag = DCD_CHECK_DATA_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd6)),
			.parameter = DCD_CHECK_DATA_PARAM_ANY_SET32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, INTR)),
			bswap((semc::semc_intr){{
				.IPCMDDONE = 1,
			}}),
		};
	} cmd6;

	/* Set SDRAM mode register. */
	struct {
		dcd_command cmd = {
			.tag = DCD_WRITE_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd7)),
			.parameter = DCD_WRITE_PARAM_WRITE32,
		};
		uint32_t args[4] = {
			bswap(SEMC_ADDR + offsetof(semc, IPTXDAT)),
			bswap(0x33),
			bswap(SEMC_ADDR + offsetof(semc, IPCMD)),
			bswap((semc::semc_ipcmd){{
				.CMD = CMD_SDRAM_MODESET,
				.KEY = 0xa55a,
			}}),
		};
	} cmd7;

	/* Wait for mode set command to complete. */
	struct {
		dcd_command cmd = {
			.tag = DCD_CHECK_DATA_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd8)),
			.parameter = DCD_CHECK_DATA_PARAM_ANY_SET32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, INTR)),
			bswap((semc::semc_intr){{
				.IPCMDDONE = 1,
			}}),
		};
	} cmd8;

	/* Enable auto refresh. */
	struct {
		dcd_command cmd = {
			.tag = DCD_WRITE_TAG,
			.length_be = __builtin_bswap16(sizeof(cmd9)),
			.parameter = DCD_WRITE_PARAM_WRITE32,
		};
		uint32_t args[2] = {
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR3)),
			bswap((semc::semc_sdramcr3){{
				.REN = 1,
				.REBL = 4,
				.PRESCALE = 11,
				.RT = 30,
				.UT = 60,
			}}),
		};
	} cmd9;
} dcd_ __attribute__((used));

static_assert(sizeof(dcd) <= 1768, "DCD is limited to 1768 bytes");
