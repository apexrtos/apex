#include <machine.h>

#include <boot.h>
#include <cpu/nxp/imxrt10xx/ccm.h>
#include <cpu/nxp/imxrt10xx/dcdc.h>
#include <cpu/nxp/imxrt10xx/iomuxc.h>
#include <cpu/nxp/imxrt10xx/semc.h>
#include <sys/dev/fsl/lpuart/regs.h>
#include <sys/include/arch.h>

#if defined(CONFIG_BOOT_CONSOLE)
static const unsigned long LPUART1 = 0x40184000;
#endif

/*
 * Setup machine state
 */
void
machine_setup(void)
{
#if defined(CONFIG_BOOT_CONSOLE)
	/* set GPIO_AD_B0_12 as LPUART1_TX */
	write32(&IOMUXC->SW_MUX_CTL_PAD_GPIO_AD_B0_12, (union iomuxc_sw_mux_ctl){
		.MUX_MODE = 2,
		.SION = SION_Software_Input_On_Disabled,
	}.r);
	write32(&IOMUXC->SW_PAD_CTL_PAD_GPIO_AD_B0_12, (union iomuxc_sw_pad_ctl){
		.SRE = SRE_Slow,
		.DSE = DSE_R0_6,
		.SPEED = SPEED_100MHz,
		.ODE = ODE_Open_Drain_Disabled,
		.PKE = PKE_Pull_Keeper_Enabled,
		.PUE = PUE_Keeper,
		.PUS = PUS_100K_Pull_Down,
		.HYS = HYS_Hysteresis_Disabled,
	}.r);

	/* set GPIO_AD_B0_13 as LPUART1_RX */
	write32(&IOMUXC->SW_MUX_CTL_PAD_GPIO_AD_B0_13, (union iomuxc_sw_mux_ctl){
		.MUX_MODE = 2,
		.SION = SION_Software_Input_On_Disabled,
	}.r);
	write32(&IOMUXC->SW_PAD_CTL_PAD_GPIO_AD_B0_12, (union iomuxc_sw_pad_ctl){
		.SRE = SRE_Slow,
		.DSE = DSE_R0_6,
		.SPEED = SPEED_100MHz,
		.ODE = ODE_Open_Drain_Disabled,
		.PKE = PKE_Pull_Keeper_Enabled,
		.PUE = PUE_Keeper,
		.PUS = PUS_100K_Pull_Down,
		.HYS = HYS_Hysteresis_Disabled,
	}.r);

	struct lpuart_regs *const u = (struct lpuart_regs*)LPUART1;

	/* configure for 115200 baud */
	write32(&u->BAUD, (union lpuart_baud){
		.SBR = 8,
		.SBNS = 0, /* one stop bit */
		.OSR = 25, /* baud = 24M / (SBR * (OSR + 1) = 115384, 0.16% error */
	}.r);
	write32(&u->CTRL, (union lpuart_ctrl){
		.PE = 0, /* parity disabled */
		.M = 0, /* 8 bit */
		.TE = 1, /* transmitter enabled */
	}.r);
#endif

	/* DRAM */
	bootinfo->ram[0].base = (void*)CONFIG_DRAM_BASE_PHYS;
	bootinfo->ram[0].size = CONFIG_DRAM_SIZE;
	bootinfo->ram[0].type = MT_NORMAL;

	/* DTCM */
	bootinfo->ram[1].base = (void*)CONFIG_DTCM_BASE_PHYS;
	bootinfo->ram[1].size = CONFIG_DTCM_SIZE;
	bootinfo->ram[1].type = MT_FAST;

	/* DMA */
	bootinfo->ram[2].base = (void*)CONFIG_DMA_BASE_PHYS;
	bootinfo->ram[2].size = CONFIG_DMA_SIZE;
	bootinfo->ram[2].type = MT_DMA;

	bootinfo->nr_rams = 3;
}

/*
 * Print one chracter
 */
void
machine_putc(int c)
{
#if defined(CONFIG_BOOT_CONSOLE)
	struct lpuart_regs *const u = (struct lpuart_regs*)LPUART1;

	while (!read32(&u->STAT).TDRE);
	write32(&u->DATA, c);
#endif
}

/*
 * Load kernel image
 */
int
machine_load_image(void)
{
	return load_a();
}

/*
 * Panic handler
 */
void
machine_panic(void)
{
	while (1);
}

/*
 * Initialise clocks
 *
 * At entry to this function the boot ROM has configured the following:
 * - CCM_ANALOG_PLL_ARM = 0x80002042
 *	DIV_SELECT = 66
 *	ENABLE = 1
 *	SOURCE = 24MHz
 * - CCM_CACRR = 0x00000001
 *	ARM_PODF = 1 (divide by 2)
 * - CCM_CBCMR = 0x35ae8304
 *	LPSPI_CLK_SEL = 0 (PLL3 PFD1)
 *	PERIPH_CLK2_SEL = 0 (pll3_sw_clk)
 *	TRACE_CLK_SEL = 2 (PLL2 PFD0)
 *	PRE_PERIPH_CLK_SEL = 3 (PLL1)
 *	LCDIF_PODF = 3 (divide by 4)
 *	LPSPI_PODF = 5 (divide by 6)
 * - CCM_CBCDR = 0x000a8200
 *	SEMC_CLK_SEL = 0 (periph_clk)
 *	SEMC_ALT_CLK_SEL = 0 (PLL2 PFD2)
 *	IPG_PODF = 2 (divide by 3)
 *	AHB_PODF = 0 (divide by 1)
 *	SEMC_PODF = 2 (divide by 3)
 *	PERIPH_CLK_SEL = 0 (pre_periph_clk_sel)
 *	PERIPH_CLK2_PODF = 0 (divide by 1)
 * - CCM_ANALOG_PLL_SYS = 0x80002001
 *	DIV_SELECT = 1
 *	ENABLE = 1
 * - CCM_ANALOG_PFD_528 = 0x18131818
 *	PFD0_FRAC = 24
 *	PFD1_FRAC = 24
 *	PFD2_FRAC = 19
 *	PFD3_FRAC = 24
 * - CCM_CSCMR1 = 0x64130001
 *	PERCLK_PODF = 1 (divide by 2)
 *	PERCLK_CLK_SEL = 0 (ipg_clk_root)
 *	SAI1_CLK_SEL = 0 (PLL3 PFD2)
 *	SAI2_CLK_SEL = 0 (PLL3 PFD2)
 *	SAI3_CLK_SEL = 0 (PLL3 PFD2)
 *	USDHC1_CLK_SEL = 1 (PLL2 PFD0)
 *	USDHC2_CLK_SEL = 1 (PLL2 PFD0)
 *	FLEXSPI_PODF = 0 (divide by 1)
 *	FLEXSPI_CLK_SEL = 3 (PLL3 PFD0)
 * - CCM_ANALOG_PLL_USB1 (PLL3) = 0x80003000
 *	DIV_SELECT = 0 (fout = fref * 20)
 *	EN_USB_CLKS = 0 (output for USBPHY off)
 *	POWER = 1
 *	ENABLE = 1
 *	BYPASS_CLK_SRC = 0 (24MHz)
 *	BYPASS = 0
 * - CCM_ANALOG_PFD_480 (PLL3) = 0x0f1a2321 (if serialClkFreq == 7)
 *				 0x0f1a231a (if serialClkFreq == 8)
 *	PFD0_FRAC = 33 (if serialClkFreq == 7), 26 (if serialClkFreq == 8)
 *	PFD1_FRAC = 35
 *	PFD2_FRAC = 26
 *	PFD3_FRAC = 15
 * - CCM_CSCDR1 = 0x06490b03
 *	UART_CLK_PODF = 3 (divide by 4)
 *	UART_CLK_SEL = 0 (pll3_80m)
 *	USDHC1_PODF = 1 (divide by 2)
 *	USDHC2_PODF = 1 (divide by 2)
 *	TRACE_PODF = 3 (divide by 4)
 * - CCM_CCSR = 0x00000100
 *	PLL3_SW_CLK_SEL = 0 (pll3_main_clk)
 * - CCM_CCGR0 = 0xc0c00fff
 * - CCM_CCGR1 = 0xfcfcc000
 * - CCM_CCGR2 = 0x0c3ff033
 * - CCM_CCGR3 = 0xf00ff300
 * - CCM_CCGR4 = 0x0000ffff
 * - CCM_CCGR5 = 0xf0033c33
 * - CCM_CCGR6 = 0x00fc3fc0
 *
 * Therefore,
 *	PLL1 = 792MHz
 *	PLL2 = 528MHz
 *	PLL2.PFD0 = 396MHz
 *	PLL2.PFD1 = 396MHz
 *	PLL2.PFD2 = 500.21Mhz
 *	PLL2.PFD3 = 396MHz
 *	PLL3 = 480MHz
 *	PLL3.PFD0 = 261.82MHz (serialClkFreq == 7), 332.31MHz (serialClkFreq == 8)
 *	PLL3.PFD1 = 246.86MHz
 *	PLL3.PFD2 = 332.31MHz
 *	PLL3.PFD3 = 576MHz
 *
 *	PERIPH_CLK = PLL1/ARM_PODF = 396MHz
 *	AHB_CLK_ROOT = PERIPH_CLK/AHB_PODF = 396MHz
 *	SEMC_CLK_ROOT = PERIPH_CLK/SEMC_PODF = 132MHz
 *	FLEXSPI_CLK_ROOT = PLL3.PFD0/FLEXSPI_PODF = 261.82MHz (serialClkFreq == 7)
 *	FLEXSPI_CLK_ROOT = PLL3.PFD0/FLEXSPI_PODF = 332.31MHz (serialClkFreq == 8)
 *	UART_CLK_ROOT = PLL3/6/UART_CLK_PODF = 20MHz
 *
 * We want to end up with:
 *	AHB_CLK_ROOT = 600MHz
 *	IPG_CLK_ROOT = 150MHz
 *	PERCLK_CLK_ROOT = 75MHz
 *	SEMC_CLK_ROOT = 166MHz
 *	FLEXSPI_CLK_ROOT = 333MHz (unchanged)
 *	UART_CLK_ROOT = 24MHz
 *	All clocks gated except what is necessary
 */
void
machine_clock_init(void)
{
	/* set core voltage to 1.25V for 600MHz operation */
	for (union dcdc_reg3 v = DCDC->REG3; v.TRG < (1250 - 800) / 25;) {
		/* step by 25mV */
		v.TRG += 1;
		write32(&DCDC->REG3, v.r);
		/* wait for core voltage to stabilise */
		while (!read32(&DCDC->REG0).STS_DC_OK);
	}

	/* bypass PLL1 (ARM PLL) and configure for 1200MHz */
	write32(&CCM_ANALOG->PLL_ARM, (union ccm_analog_pll_arm){
		.DIV_SELECT = 100, /* fout = 24MHz * DIV_SELECT / 2 */
		.POWERDOWN = 0,
		.ENABLE = 1,
		.BYPASS_CLK_SRC = BYPASS_CLK_SRC_REF_CLK_24M,
		.BYPASS = 1,
	}.r);

	/* set SEMC clock source to PLL2.PFD2/3 = 166MHz */
	write32(&CCM->CBCDR, (union ccm_cbcdr){
		.SEMC_CLK_SEL = SEMC_CLK_SEL_ALTERNATE,
		.SEMC_ALT_CLK_SEL = SEMC_ALT_CLK_SEL_PLL2_PFD2,
		.IPG_PODF = 3, /* divide by 4 */
		.AHB_PODF = 0, /* divide by 1 */
		.SEMC_PODF = 2, /* divide by 3 */
		.PERIPH_CLK_SEL = PERIPH_CLK_SEL_PRE_PERIPH,
		.PERIPH_CLK2_PODF = 0, /* divide by 1 */
	}.r);

	/* wait for PLL1 to stabilise */
	while (!read32(&CCM_ANALOG->PLL_ARM).LOCK);

	/* unbypass PLL1 */
	write32(&CCM_ANALOG->PLL_ARM, (union ccm_analog_pll_arm){
		.DIV_SELECT = 100, /* fout = 24MHz * DIV_SELECT / 2 */
		.POWERDOWN = 0,
		.ENABLE = 1,
		.BYPASS_CLK_SRC = BYPASS_CLK_SRC_REF_CLK_24M,
		.BYPASS = 0,
	}.r);

	/* configure UART_CLK_ROOT */
	write32(&CCM->CSCDR1, (union ccm_cscdr1){
		.UART_CLK_PODF = 0,
		.UART_CLK_SEL = UART_CLK_SEL_osc_clk,
		.USDHC1_PODF = 1,
		.USDHC2_PODF = 1,
		.TRACE_PODF = 3,
	}.r);

	/* TODO: gate all unnecessary clocks */
	write32(&CCM->CCGR0, 0xffffffff);
	write32(&CCM->CCGR1, 0xffffffff);
	write32(&CCM->CCGR2, 0xffffffff);
	write32(&CCM->CCGR3, 0xffffffff);
	write32(&CCM->CCGR4, 0xffffffff);
	write32(&CCM->CCGR5, 0xffffffff);
	write32(&CCM->CCGR6, 0xffffffff);
}

/*
 * Configure SDRAM
 */
void
machine_memory_init(void)
{
	/* pad multiplexing */
	for (size_t i = 0; i < 42; ++i) {
		write32(&IOMUXC->SW_MUX_CTL_PAD_GPIO_EMC[i], (union iomuxc_sw_mux_ctl){
			.MUX_MODE = 0,
			.SION = SION_Software_Input_On_Disabled,
		}.r);
	}
	/* DQS loopback from pad */
	write32(&IOMUXC->SW_MUX_CTL_PAD_GPIO_EMC_39, (union iomuxc_sw_mux_ctl){
		.MUX_MODE = 0,
		.SION = SION_Software_Input_On_Enabled
	}.r);

	/* pad control */
	for (size_t i = 0; i < 42; ++i) {
		write32(&IOMUXC->SW_PAD_CTL_PAD_GPIO_EMC[i], (union iomuxc_sw_pad_ctl){
			.SRE = SRE_Fast,
			.DSE = DSE_R0_7,
			.SPEED = SPEED_200MHz,
			.ODE = ODE_Open_Drain_Disabled,
			.PKE = PKE_Pull_Keeper_Enabled,
			.PUE = PUE_Keeper,
			.PUS = PUS_100K_Pull_Down,
			.HYS = HYS_Hysteresis_Enabled
		}.r);
	}

	/* SEMC */
	write32(&SEMC->MCR, (union semc_mcr){
		.DQSMD = DQSMD_from_pad,
		.BTO = 16,
	}.r);
	write32(&SEMC->BMCR0, (union semc_bmcr0){
		.WQOS = 4,
		.WAGE = 2,
		.WSH = 5,
		.WRWS = 3,
	}.r);
	write32(&SEMC->BMCR1, (union semc_bmcr1){
		.WQOS = 4,
		.WAGE = 2,
		.WPH = 5,
		.WRWS = 3,
		.WBR = 6,
	}.r);
	write32(&SEMC->IOCR, (union semc_iocr){
		.MUX_A8 = 0,	/* SDRAM Address bit (A8) */
		.MUX_CSX0 = 5,	/* NOR CE# */
		.MUX_CSX1 = 6,	/* PSRAM CE# */
		.MUX_CSX2 = 4,	/* NAND CE# */
		.MUX_CSX3 = 7,	/* DBI CSX */
		.MUX_RDY = 0,	/* NAND Ready/Wait# input */
	}.r);
	write32(&SEMC->BR0, (union semc_br){
		.VLD = 1,
		.MS = 13,	/* 32MiB */
		.BA = 0x80000,
	}.r);
	write32(&SEMC->SDRAMCR0, (union semc_sdramcr0){
		.PS = 1,	/* 16 bit */
		.BL = 3,	/* burst length 8 */
		.COL = 3,	/* 9 bit columns */
		.CL = 3,	/* CAS latency 3 */
	}.r);
	write32(&SEMC->SDRAMCR1, (union semc_sdramcr1){
		.PRE2ACT = 2,
		.ACT2RW = 2,
		.RFRC = 9,
		.WRC = 1,
		.CKEOFF = 5,
		.ACT2PRE = 6,
	}.r);
	write32(&SEMC->SDRAMCR2, (union semc_sdramcr2){
		.SRRC = 32,
		.REF2REF = 9,
		.ACT2ACT = 1,
		.ITO = 0,
	}.r);

	/* send commands to SDRAM */
	write32(&SEMC->IPCR0, (union semc_ipcr0){
		.SA = 0x80000000,
	}.r);
	write32(&SEMC->IPCR1, (union semc_ipcr1){
		.DATASZ = 2,
	}.r);
	write32(&SEMC->IPCR2, (union semc_ipcr2){
		.BM0 = 0,
		.BM1 = 0,
		.BM2 = 0,
		.BM3 = 0,
	}.r);

	/* issue precharge all command */
	write32(&SEMC->IPCMD, (union semc_ipcmd){
		.CMD = CMD_SDRAM_PRECHARGE_ALL,
		.KEY = 0xa55a,
	}.r);
	while (!read32(&SEMC->INTR).IPCMDDONE);

	/* issue two auto refresh commands */
	write32(&SEMC->IPCMD, (union semc_ipcmd){
		.CMD = CMD_SDRAM_AUTO_REFRESH,
		.KEY = 0xa55a,
	}.r);
	while (!read32(&SEMC->INTR).IPCMDDONE);
	write32(&SEMC->IPCMD, (union semc_ipcmd){
		.CMD = CMD_SDRAM_AUTO_REFRESH,
		.KEY = 0xa55a,
	}.r);
	while (!read32(&SEMC->INTR).IPCMDDONE);

	/* set mode register */
	write32(&SEMC->IPTXDAT, 0x33);
	write32(&SEMC->IPCMD, (union semc_ipcmd){
		.CMD = CMD_SDRAM_MODESET,
		.KEY = 0xa55a,
	}.r);
	while (!read32(&SEMC->INTR).IPCMDDONE);

	/* enable auto refresh */
	write32(&SEMC->SDRAMCR3, (union semc_sdramcr3){
		.REN = 1,
		.REBL = 4,
		.PRESCALE = 10,
		.RT = 33,
		.UT = 80,
	}.r);
}
