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

template<class T>
constexpr uint32_t bswap(T r)
{
	return bswap(r.r);
}

constexpr uint32_t bswap(ccm::cbcdr v)
{
	uint32_t tmp =
	    (uint32_t)v.SEMC_CLK_SEL << 6 |
	    (uint32_t)v.SEMC_ALT_CLK_SEL << 7 |
	    v.IPG_PODF << 8 |
	    v.AHB_PODF << 10 |
	    v.SEMC_PODF << 16 |
	    (uint32_t)v.PERIPH_CLK_SEL << 25 |
	    v.PERIPH_CLK2_PODF << 27;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::mcr v)
{
	uint32_t tmp =
	    v.SWRST << 0 |
	    v.MDIS << 1 |
	    (uint32_t)v.DQSMD << 2 |
	    v.WPOL0 << 6 |
	    v.WPOL1 << 7 |
	    v.CTO << 16 |
	    v.BTO << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::bmcr0 v)
{
	uint32_t tmp =
	    v.WQOS << 0 |
	    v.WAGE << 4 |
	    v.WSH << 8 |
	    v.WRWS << 16;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::bmcr1 v)
{
	uint32_t tmp =
	    v.WQOS << 0 |
	    v.WAGE << 4 |
	    v.WPH << 8 |
	    v.WRWS << 16 |
	    v.WBR << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::iocr v)
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

constexpr uint32_t bswap(semc::br v)
{
	uint32_t tmp =
	    v.VLD << 0 |
	    v.MS << 1 |
	    v.BA << 12;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::sdramcr0 v)
{
	uint32_t tmp =
	    v.PS << 0 |
	    v.BL << 4 |
	    v.COL << 8 |
	    v.CL << 10;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::sdramcr1 v)
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

constexpr uint32_t bswap(semc::sdramcr2 v)
{
	uint32_t tmp =
	    v.SRRC << 0 |
	    v.REF2REF << 8 |
	    v.ACT2ACT << 16 |
	    v.ITO << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::sdramcr3 v)
{
	uint32_t tmp =
	    v.REN << 0 |
	    v.REBL << 1 |
	    v.PRESCALE << 8 |
	    v.RT << 16 |
	    v.UT << 24;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::ipcr0 v)
{
	return bswap(v.SA);
}

constexpr uint32_t bswap(semc::ipcr1 v)
{
	return bswap(v.DATSZ);
}

constexpr uint32_t bswap(semc::ipcr2 v)
{
	uint32_t tmp =
	    v.BM0 << 0 |
	    v.BM1 << 1 |
	    v.BM2 << 2 |
	    v.BM3 << 3;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::ipcmd v)
{
	uint32_t tmp =
	    (uint32_t)v.CMD << 0 |
	    v.KEY << 16;
	return bswap(tmp);
}

constexpr uint32_t bswap(semc::intr v)
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
constexpr uint32_t semc_pad_control = bswap(iomuxc::sw_pad_ctl(
	iomuxc::sw_pad_ctl::sre::Fast,
	iomuxc::sw_pad_ctl::dse::R0_7,
	iomuxc::sw_pad_ctl::speed::MHz_200,
	iomuxc::sw_pad_ctl::ode::Open_Drain_Disabled,
	iomuxc::sw_pad_ctl::pke::Pull_Keeper_Enabled,
	iomuxc::sw_pad_ctl::pue::Keeper,
	iomuxc::sw_pad_ctl::pus::Pull_Down_100K,
	iomuxc::sw_pad_ctl::hys::Hysteresis_Enabled
));

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
			bswap((ccm::cbcdr){{
				.SEMC_CLK_SEL = ccm::cbcdr::semc_clk_sel::ALTERNATE,
				.SEMC_ALT_CLK_SEL = ccm::cbcdr::semc_alt_clk_sel::PLL2_PFD2,
				.IPG_PODF = 3, /* divide by 4 */
				.AHB_PODF = 0, /* divide by 1 */
				.SEMC_PODF = 2, /* divide by 3 */
				.PERIPH_CLK_SEL = ccm::cbcdr::periph_clk_sel::PRE_PERIPH,
				.PERIPH_CLK2_PODF = 0, /* divide by 1 */
			}}),

			/* Ungate SEMC clock */
			bswap(CCM_ADDR + offsetof(ccm, CCGR3)),
			bswap(0xf00ff330),

			/* Configure SEMC pad multiplexing. */
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[0])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[1])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[2])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[3])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[4])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[5])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[6])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[7])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[8])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[9])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[10])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[11])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[12])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[13])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[14])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[15])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[16])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[17])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[18])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[19])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[20])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[21])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[22])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[23])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[24])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[25])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[26])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[27])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[28])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[29])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[30])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[31])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[32])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[33])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[34])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[35])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[36])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[37])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[38])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[39])),
			bswap(iomuxc::sw_mux_ctl(0, iomuxc::sw_mux_ctl::sion::Software_Input_On_Enabled)),
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[40])), 0,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_MUX_CTL_PAD_GPIO_EMC[41])), 0,

			/* Configure SEMC pad control registers */
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[0])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[1])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[2])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[3])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[4])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[5])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[6])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[7])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[8])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[9])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[10])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[11])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[12])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[13])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[14])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[15])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[16])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[17])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[18])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[19])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[20])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[21])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[22])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[23])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[24])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[25])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[26])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[27])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[28])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[29])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[30])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[31])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[32])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[33])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[34])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[35])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[36])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[37])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[38])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[39])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[40])), semc_pad_control,
			bswap(IOMUXC_ADDR + offsetof(iomuxc, SW_PAD_CTL_PAD_GPIO_EMC[41])), semc_pad_control,

			/* Configure DRAM on SEMC module. */
			bswap(SEMC_ADDR + offsetof(semc, MCR)),
			bswap((semc::mcr){{
				.SWRST = 0,
				.MDIS = 0,
				.DQSMD = semc::mcr::dqsmd::FROM_PAD,
				.WPOL0 = 0,
				.WPOL1 = 0,
				.CTO = 0,
				.BTO = 16,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, IOCR)),
			bswap((semc::iocr){{
				.MUX_A8 = 0,	/* SDRAM Address bit (A8) */
				.MUX_CSX0 = 5,	/* NOR CE# */
				.MUX_CSX1 = 6,	/* PSRAM CE# */
				.MUX_CSX2 = 4,	/* NAND CE# */
				.MUX_CSX3 = 7,	/* DBI CSX */
				.MUX_RDY = 0,	/* NAND Ready/Wait# input */
			}}),
			bswap(SEMC_ADDR + offsetof(semc, BMCR0)),
			bswap((semc::bmcr0){{
				.WQOS = 1,
				.WAGE = 8,
				.WSH = 0,
				.WRWS = 0,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, BMCR1)),
			bswap((semc::bmcr1){{
				.WQOS = 1,
				.WAGE = 8,
				.WPH = 0,
				.WRWS = 0,
				.WBR = 0,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, BR0)),
			bswap((semc::br){{
				.VLD = 1,
				.MS = 13,	/* 32MiB */
				.BA = 0x80000,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR0)),
			bswap((semc::sdramcr0){{
				.PS = 1,	/* 16 bit */
				.BL = 3,	/* burst length 8 */
				.COL = 3,	/* 9 bit columns */
				.CL = 3,	/* CAS latency 3 */
			}}),
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR1)),
			bswap((semc::sdramcr1){{
				.PRE2ACT = 2,
				.ACT2RW = 2,
				.RFRC = 9,
				.WRC = 1,
				.CKEOFF = 0,
				.ACT2PRE = 6,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, SDRAMCR2)),
			bswap((semc::sdramcr2){{
				.SRRC = 10,
				.REF2REF = 9,
				.ACT2ACT = 9,
				.ITO = 0,
			}}),

			/* Prepare to send DRAM commands. */
			bswap(SEMC_ADDR + offsetof(semc, IPCR0)),
			bswap((semc::ipcr0){{
				.SA = 0x80000000,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, IPCR1)),
			bswap((semc::ipcr1){{
				.DATSZ = 2,
			}}),
			bswap(SEMC_ADDR + offsetof(semc, IPCR2)),
			bswap((semc::ipcr2){{
				.BM0 = 0,
				.BM1 = 0,
				.BM2 = 0,
				.BM3 = 0,
			}}),

			/* Issue precharge all command. */
			bswap(SEMC_ADDR + offsetof(semc, IPCMD)),
			bswap((semc::ipcmd){{
				.CMD = semc::ipcmd::cmd::PRECHARGE_ALL,
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
			bswap((semc::intr){{
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
			bswap((semc::ipcmd){{
				.CMD = semc::ipcmd::cmd::AUTO_REFRESH,
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
			bswap((semc::intr){{
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
			bswap((semc::ipcmd){{
				.CMD = semc::ipcmd::cmd::AUTO_REFRESH,
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
			bswap((semc::intr){{
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
			bswap(0x33u),
			bswap(SEMC_ADDR + offsetof(semc, IPCMD)),
			bswap((semc::ipcmd){{
				.CMD = semc::ipcmd::cmd::MODESET,
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
			bswap((semc::intr){{
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
			bswap((semc::sdramcr3){{
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
