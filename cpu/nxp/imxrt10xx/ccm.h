#pragma once

#include <cassert>
#include <cstdint>

/*
 * Clock Control Module
 */
struct ccm {
	enum class cg : unsigned {
		OFF = 0,
		RUN = 1,
		ON = 3,
	};

	uint32_t CCR;
	uint32_t : 32;
	uint32_t CSR;
	uint32_t CCSR;
	uint32_t CACRR;
	union cbcdr {
		enum class semc_clk_sel : unsigned {
			PERIPH,
			ALTERNATE,
		};

		enum class semc_alt_clk_sel : unsigned {
			PLL2_PFD2,
			PLL3_PFD1,
		};

		enum class periph_clk_sel : unsigned {
			PRE_PERIPH,
			PERIPH_CLK2,
		};

		struct {
			uint32_t : 6;
			semc_clk_sel SEMC_CLK_SEL : 1;
			semc_alt_clk_sel SEMC_ALT_CLK_SEL : 1;
			uint32_t IPG_PODF : 2;
			uint32_t AHB_PODF : 3;
			uint32_t : 3;
			uint32_t SEMC_PODF : 3;
			uint32_t : 6;
			periph_clk_sel PERIPH_CLK_SEL : 1;
			uint32_t : 1;
			uint32_t PERIPH_CLK2_PODF : 3;
			uint32_t : 2;
		};
		uint32_t r;
	} CBCDR;
	union cbcmr {
		enum class lpspi_clk_sel : unsigned {
			PLL3_PFD1,
			PLL3_PFD0,
			PLL2,
			PLL2_PFD2,
		};

		enum class flexspi2_clk_sel : unsigned {
			PLL2_PFD2,
			PLL3_PFD0,
			PLL3_PFD1,
			PLL2
		};

		enum class periph_clk2_sel : unsigned {
			PLL3_SW_CLK,
			OSC_CLK,
			PLL2_BYPASS_CLK,
		};

		enum class trace_clk_sel : unsigned {
			PLL2,
			PLL2_PFD2,
			PLL2_PFD0,
			PLL2_PFD1,
		};

		enum class pre_periph_clk_sel : unsigned {
			PLL2,
			PLL2_PFD2,
			PLL2_PFD0,
			PLL1,
		};

		struct {
			uint32_t : 4;
			lpspi_clk_sel LPSPI_CLK_SEL : 2;
			uint32_t : 2;
			flexspi2_clk_sel FLEXSPL2_CLK_SEL : 2; /* For IMXRT106x only. RSV for IMXRT105x */
			uint32_t : 2;
			periph_clk2_sel PERIPH_CLK2_SEL : 2;
			trace_clk_sel TRACE_CLK_SEL : 2;
			uint32_t : 2;
			pre_periph_clk_sel PRE_PERIPH_CLK_SEL : 2;
			uint32_t : 3;
			uint32_t LCDIF_PODF : 3;
			uint32_t LPSPI_PODF : 3;
			uint32_t FLEXSPI2_PODF : 3; /* For IMXRT106x only. RSV for IMXRT105x */
		};
		uint32_t r;
	} CBCMR;
	union cscmr1 {
		enum class perclk_clk_sel : unsigned {
			IPG_CLK_ROOT,
			OSC_CLK,
		};

		enum class sai1_clk_sel : unsigned {
			SEL_PLL3_PFD2,
			SEL_PLL5,
			SEL_PLL4,
		};

		enum class sai2_clk_sel : unsigned {
			PLL3_PFD2,
			PLL5,
			PLL4,
		};

		enum class sai3_clk_sel : unsigned {
			PLL3_PFD2,
			PLL5,
			PLL4,
		};

		enum class usdhc1_clk_sel : unsigned {
			PLL2_PFD2,
			PLL2_PFD0,
		};

		enum class usdhc2_clk_sel : unsigned {
			PLL2_PFD2,
			PLL2_PFD0,
		};

		enum class flexspi_clk_sel : unsigned {
			SEMC_CLK_ROOT_PRE,
			PLL3_SW_CLK,
			PLL2_PFD2,
			PLL3_PFD0,
		};

		struct {
			uint32_t PERCLK_PODF : 6;
			perclk_clk_sel PERCLK_CLK_SEL : 1;
			uint32_t : 3;
			sai1_clk_sel SAI1_CLK_SEL : 2;
			sai2_clk_sel SAI2_CLK_SEL : 2;
			sai3_clk_sel SAI3_CLK_SEL : 2;
			usdhc1_clk_sel USDHC1_CLK_SEL : 1;
			usdhc2_clk_sel USDHC2_CLK_SEL : 1;
			uint32_t : 5;
			uint32_t FLEXSPI_PODF : 3;
			uint32_t : 3;
			flexspi_clk_sel FLEXSPI_CLK_SEL : 2;
			uint32_t : 1;
		};
		uint32_t r;
	} CSCMR1;
	union cscmr2 {
		enum class can_clk_sel : unsigned {
			PLL3_SW_CLK_60M,
			OSC_CLK,
			PLL3_SW_CLK_80M,
			DISABLE,
		};

		enum class flexio2_clk_sel : unsigned {
			PLL4,
			PLL3_PFD2,
			PLL5,
			PLL3_SW_CLK,
		};

		struct {
			uint32_t : 2;
			uint32_t CAN_CLK_PODF : 6;
			can_clk_sel CAN_CLK_SEL : 2;
			uint32_t : 9;
			flexio2_clk_sel FLEXIO2_CLK_SEL : 2;
			uint32_t : 11;
		};
		uint32_t r;
	} CSCMR2;
	union cscdr1 {
		enum class uart_clk_sel : unsigned {
			PLL3_80M,
			OSC_CLK,
		};

		struct {
			uint32_t UART_CLK_PODF : 6;
			uart_clk_sel UART_CLK_SEL : 1;
			uint32_t : 4;
			uint32_t USDHC1_PODF : 3;
			uint32_t : 2;
			uint32_t USDHC2_PODF : 3;
			uint32_t : 6;
			uint32_t TRACE_PODF : 2;
			uint32_t : 5;
		};
		uint32_t r;
	} CSCDR1;
	uint32_t CS1CDR;
	uint32_t CS2CDR;
	uint32_t CDCDR;
	uint32_t : 32;
	uint32_t CSCDR2;
	uint32_t CSCDR3;
	uint32_t : 32;
	uint32_t : 32;
	union cdhipr {
		struct {
			uint32_t SEMC_PODF_BUSY : 1;
			uint32_t AHB_PODF_BUSY : 1;
			uint32_t : 1;
			uint32_t PERIPH2_CLK_SEL_BUSY : 1;
			uint32_t : 1;
			uint32_t PERIPH_CLK_SEL_BUSY : 1;
			uint32_t : 10;
			uint32_t ARM_PODF_BUSY : 1;
			uint32_t : 15;
		};
		uint32_t r;
	} CDHIPR;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t CLPCR;
	uint32_t CISR;
	uint32_t CIMR;
	uint32_t CCOSR;
	uint32_t CGPR;
	union ccgr0 {
		struct {
			cg aips_tz1 : 2;
			cg aips_tz2 : 2;
			cg mqs : 2;
			cg flexspi_exsc : 2; /* For IMXRT105x only. RSV for IMXRT106x */
			cg sim_main : 2;
			cg dcp : 2;
			cg lpuart3 : 2;
			cg can1 : 2;
			cg can1_serial : 2;
			cg can2 : 2;
			cg can2_serial : 2;
			cg trace : 2;
			cg gpt2_bus : 2;
			cg gpt2_serial : 2;
			cg lpuart2 : 2;
			cg gpio2 : 2;
		};
		uint32_t r;
	} CCGR0;
	union ccgr1 {
		struct {
			cg lpspi1 : 2;
			cg lpspi2 : 2;
			cg lpspi3 : 2;
			cg lpspi4 : 2;
			cg adc2 : 2;
			cg enet : 2;
			cg pit : 2;
			cg aoi2 : 2;
			cg adc1 : 2;
			cg semc_exsc : 2;
			cg gpt1_bus : 2;
			cg gpt1_serial : 2;
			cg lpuart4 : 2;
			cg gpio1 : 2;
			cg csu : 2;
			cg gpio5: 2; /* For IMXRT106x only. RSV for IMXRT105x */
		};
		uint32_t r;
	} CCGR1;
	union ccgr2 {
		struct {
			cg ocram_exsc : 2;
			cg csi : 2;
			cg iomuxc_snvs : 2;
			cg lpi2c1 : 2;
			cg lpi2c2 : 2;
			cg lpi2c3 : 2;
			cg ocotp : 2; /* OCOTP for IMXRT106x. IIM for IMXRT105x. Functionally the same */
			cg xbar3 : 2;
			cg ipmux1 : 2;
			cg ipmux2 : 2;
			cg ipmux3 : 2;
			cg xbar1 : 2;
			cg xbar2 : 2;
			cg gpio3 : 2;
			cg lcd : 2;
			cg pxp : 2;
		};
		uint32_t r;
	} CCGR2;
	union ccgr3 {
		struct {
			cg flexio2 : 2;
			cg lpuart5 : 2;
			cg semc : 2;
			cg lpuart6 : 2;
			cg aoi1 : 2;
			cg lcdif_pix : 2;
			cg gpio4 : 2;
			cg ewm : 2;
			cg wdog1 : 2;
			cg flexram : 2;
			cg acmp1 : 2;
			cg acmp2 : 2;
			cg acmp3 : 2;
			cg acmp4 : 2;
			cg ocram : 2;
			cg iomux_snvs_gpr : 2;
		};
		uint32_t r;
	} CCGR3;
	union ccgr4 {
		struct {
			cg sim_m7_mainclk : 2;
			cg iomuxc : 2;
			cg iomuxc_gpr : 2;
			cg bee : 2;
			cg sim_m7 : 2;
			cg tsc_dig : 2;
			cg sim_m : 2;
			cg sim_ems : 2;
			cg pwm1 : 2;
			cg pwm2 : 2;
			cg pwm3 : 2;
			cg pwm4 : 2;
			cg enc1 : 2;
			cg enc2 : 2;
			cg enc3 : 2;
			cg enc4 : 2;
		};
		uint32_t r;
	} CCGR4;
	union ccgr5 {
		struct {
			cg rom : 2;
			cg flexio1 : 2;
			cg wdog3 : 2;
			cg dma : 2;
			cg kpp : 2;
			cg wdog2 : 2;
			cg aips_tz4 : 2;
			cg spdif : 2;
			cg sim_main : 2;
			cg sai1 : 2;
			cg sai2 : 2;
			cg sai3 : 2;
			cg lpuart1 : 2;
			cg lpuart7 : 2;
			cg snvs_hp : 2;
			cg snvs_lp : 2;
		};
		uint32_t r;
	} CCGR5;
	union ccgr6 {
		struct {
			cg usboh3 : 2;
			cg usdhc1 : 2;
			cg usdhc2 : 2;
			cg dcdc : 2;
			cg ipmux4 : 2;
			cg flexspi : 2;
			cg trng : 2;
			cg lpuart8 : 2;
			cg timer4 : 2;
			cg aips_tz3 : 2;
			cg sim_per : 2;
			cg anadig : 2;
			cg lpi2c4 : 2;
			cg timer1 : 2;
			cg timer2 : 2;
			cg timer3 : 2;
		};
		uint32_t r;
	} CCGR6;
	union ccgr7 {
		struct {
			cg enet2 : 2;
			cg flexspi2 : 2;
			cg axbs : 2;
			cg can3 : 2;
			cg can3_serial : 2;
			cg aips_lite : 2;
			cg flexio3 : 2;
			uint32_t : 18;
		};
		uint32_t r;
	} CCGR7; /* For IMXRT106x only. RSV for IMXRT105x */
	uint32_t CMEOR;
};
static_assert(sizeof(ccm) == 0x8c, "");

struct ccm_analog {
	enum class bypass_clk_src : unsigned {
		REF_CLK_24M,
		CLK1,
	};

	union pll_arm {
		struct {
			uint32_t DIV_SELECT : 7;
			uint32_t : 5;
			uint32_t POWERDOWN : 1;
			uint32_t ENABLE : 1;
			bypass_clk_src BYPASS_CLK_SRC : 2;
			uint32_t BYPASS : 1;
			uint32_t : 2;
			uint32_t PLL_SEL : 1;
			uint32_t : 11;
			uint32_t LOCK : 1;
		};
		uint32_t r;
	};

	union pll_usb {
		struct {
			uint32_t : 1;
			uint32_t DIV_SELECT : 1;
			uint32_t : 4;
			uint32_t EN_USB_CLKS : 1;
			uint32_t : 5;
			uint32_t POWER : 1;
			uint32_t ENABLE : 1;
			bypass_clk_src BYPASS_CLK_SRC : 2;
			uint32_t BYPASS : 1;
			uint32_t : 14;
			uint32_t LOCK : 1;
		};
		uint32_t r;
	};

	union pll_sys {
		struct {
			uint32_t DIV_SELECT : 1;
			uint32_t : 11;
			uint32_t POWERDOWN : 1;
			uint32_t ENABLE : 1;
			bypass_clk_src BYPASS_CLK_SRC : 2;
			uint32_t BYPASS : 1;
			uint32_t : 14;
			uint32_t LOCK : 1;
		};
		uint32_t r;
	};

	union pfd {
		struct {
			uint32_t PFD0_FRAC : 6;
			uint32_t PFD0_STABLE : 1;
			uint32_t PFD0_CLKGATE : 1;
			uint32_t PFD1_FRAC : 6;
			uint32_t PFD1_STABLE : 1;
			uint32_t PFD1_CLKGATE : 1;
			uint32_t PFD2_FRAC : 6;
			uint32_t PFD2_STABLE : 1;
			uint32_t PFD2_CLKGATE : 1;
			uint32_t PFD3_FRAC : 6;
			uint32_t PFD3_STABLE : 1;
			uint32_t PFD3_CLKGATE : 1;
		};
		uint32_t r;
	};

	pll_arm PLL_ARM;
	pll_arm PLL_ARM_SET;
	pll_arm PLL_ARM_CLR;
	pll_arm PLL_ARM_TOG;
	pll_usb PLL_USB1;
	pll_usb PLL_USB1_SET;
	pll_usb PLL_USB1_CLR;
	pll_usb PLL_USB1_TOG;
	pll_usb PLL_USB2;
	pll_usb PLL_USB2_SET;
	pll_usb PLL_USB2_CLR;
	pll_usb PLL_USB2_TOG;
	pll_sys PLL_SYS;
	pll_sys PLL_SYS_SET;
	pll_sys PLL_SYS_CLR;
	pll_sys PLL_SYS_TOG;
	uint32_t PLL_SYS_SS;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_SYS_NUM;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_SYS_DENOM;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_AUDIO;
	uint32_t PLL_AUDIO_SET;
	uint32_t PLL_AUDIO_CLR;
	uint32_t PLL_AUDIO_TOG;
	uint32_t PLL_AUDIO_NUM;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_AUDIO_DENOM;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_VIDEO;
	uint32_t PLL_VIDEO_SET;
	uint32_t PLL_VIDEO_CLR;
	uint32_t PLL_VIDEO_TOG;
	uint32_t PLL_VIDEO_NUM;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_VIDEO_DENOM;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t PLL_ENET;
	uint32_t PLL_ENET_SET;
	uint32_t PLL_ENET_CLR;
	uint32_t PLL_ENET_TOG;
	pfd PFD_480;
	pfd PFD_480_SET;
	pfd PFD_480_CLR;
	pfd PFD_480_TOG;
	pfd PFD_528;
	pfd PFD_528_SET;
	pfd PFD_528_CLR;
	pfd PFD_528_TOG;
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
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t MISC0;
	uint32_t MISC0_SET;
	uint32_t MISC0_CLR;
	uint32_t MISC0_TOG;
	uint32_t MISC1;
	uint32_t MISC1_SET;
	uint32_t MISC1_CLR;
	uint32_t MISC1_TOG;
	uint32_t MISC2;
	uint32_t MISC2_SET;
	uint32_t MISC2_CLR;
	uint32_t MISC2_TOG;
};
static_assert(sizeof(ccm_analog) == 0x180, "");

#define CCM_ADDR 0x400fc000
#define CCM_ANALOG_ADDR 0x400d8000
static ccm *const CCM = (ccm*)CCM_ADDR;
static ccm_analog *const CCM_ANALOG = (ccm_analog*)CCM_ANALOG_ADDR;
