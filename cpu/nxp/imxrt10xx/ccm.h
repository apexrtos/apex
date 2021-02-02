#ifndef imxrt10xx_ccm_h
#define imxrt10xx_ccm_h

#include <assert.h>
#include <stdint.h>

/*
 * Clock Control Module
 */
enum ccm_cg {
	CCM_CG_OFF = 0,
	CCM_CG_RUN = 1,
	CCM_CG_ON = 3,
};

enum ccm_cbcdr_semc_clk_sel {
	SEMC_CLK_SEL_PERIPH,
	SEMC_CLK_SEL_ALTERNATE,
};

enum ccm_cbcdr_semc_alt_clk_sel {
	SEMC_ALT_CLK_SEL_PLL2_PFD2,
	SEMC_ALT_CLK_SEL_PLL3_PFD1,
};

enum ccm_cbcdr_periph_clk_sel {
	PERIPH_CLK_SEL_PRE_PERIPH,
	PERIPH_CLK_SEL_PERIPH_CLK2,
};

enum ccm_cbcmr_lpspi_clk_sel {
	LPSPI_CLK_SEL_PLL3_PFD1,
	LPSPI_CLK_SEL_PLL3_PFD0,
	LPSPI_CLK_SEL_PLL2,
	LPSPI_CLK_SEL_PLL2_PFD2,
};

enum ccm_cmcmr_flexspi2_clk_sel {
	FLEXSPI2_CLK_SEL_PLL2_PFD2,
	FLEXSPI2_CLK_SEL_PLL3_PFD0,
	FLEXSPI2_CLK_SEL_PLL3_PFD1,
	FLEXSPI2_CLK_SEL_PLL2
};

enum ccm_cbcmr_periph_clk2_sel {
	PERIPH_CLK2_SEL_PLL3_SW_CLK,
	PERIPH_CLK2_SEL_OSC_CLK,
	PERIPH_CLK2_SEL_PLL2_BYPASS_CLK,
};

enum ccm_cbcmr_trace_clk_sel {
	TRACE_CLK_SEL_PLL2,
	TRACE_CLK_SEL_PLL2_PFD2,
	TRACE_CLK_SEL_PLL2_PFD0,
	TRACE_CLK_SEL_PLL2_PFD1,
};

enum ccm_cbcmr_pre_periph_clk_sel {
	PRE_PERIPH_CLK_SEL_PLL2,
	PRE_PERIPH_CLK_SEL_PLL2_PFD2,
	PRE_PERIPH_CLK_SEL_PLL2_PFD0,
	PRE_PERIPH_CLK_SEL_PLL1,
};

enum ccm_cscmr1_perclk_clk_sel {
	PERCLK_CLK_SEL_IPG_CLK_ROOT,
	PERCLK_CLK_SEL_OSC_CLK,
};

enum ccm_cscmr1_sai1_clk_sel {
	SAI1_CLK_SEL_PLL3_PFD2,
	SAI1_CLK_SEL_PLL5,
	SAI1_CLK_SEL_PLL4,
};

enum ccm_cscmr1_sai2_clk_sel {
	SAI2_CLK_SEL_PLL3_PFD2,
	SAI2_CLK_SEL_PLL5,
	SAI2_CLK_SEL_PLL4,
};

enum ccm_cscmr1_sai3_clk_sel {
	SAI3_CLK_SEL_PLL3_PFD2,
	SAI3_CLK_SEL_PLL5,
	SAI3_CLK_SEL_PLL4,
};

enum ccm_cscmr1_usdhc1_clk_sel {
	USDHC1_CLK_SEL_PLL2_PFD2,
	USDHC1_CLK_SEL_PLL2_PFD0,
};

enum ccm_cscmr1_usdhc2_clk_sel {
	USDHC2_CLK_SEL_PLL2_PFD2,
	USDHC2_CLK_SEL_PLL2_PFD0,
};

enum ccm_cscmr1_flexspi_clk_sel {
	FLEXSPI_CLK_SEL_SEMC_CLK_ROOT_PRE,
	FLEXSPI_CLK_SEL_PLL3_SW_CLK,
	FLEXSPI_CLK_SEL_PLL2_PFD2,
	FLEXSPI_CLK_SEL_PLL3_PFD0,
};

enum ccm_cscmr2_can_clk_sel {
	CAN_CLK_SEL_PLL3_SW_CLK_60M,
	CAN_CLK_SEL_OSC_CLK,
	CAN_CLK_SEL_PLL3_SW_CLK_80M,
	CAN_CLK_SEL_DISABLE,
};

enum ccm_cscmr2_flexio2_clk_sel {
	FLEXIO2_CLK_SEL_PLL4,
	FLEXIO2_CLK_SEL_PLL3_PFD2,
	FLEXIO2_CLK_SEL_PLL5,
	FLEXIO2_CLK_SEL_PLL3_SW_CLK,
};

enum ccm_cscdr1_uart_clk_sel {
	UART_CLK_SEL_pll3_80m,
	UART_CLK_SEL_osc_clk,
};

struct ccm {
	uint32_t CCR;
	uint32_t : 32;
	uint32_t CSR;
	uint32_t CCSR;
	uint32_t CACRR;
	union ccm_cbcdr {
		struct {
			uint32_t : 6;
			enum ccm_cbcdr_semc_clk_sel SEMC_CLK_SEL : 1;
			enum ccm_cbcdr_semc_alt_clk_sel SEMC_ALT_CLK_SEL : 1;
			uint32_t IPG_PODF : 2;
			uint32_t AHB_PODF : 3;
			uint32_t : 3;
			uint32_t SEMC_PODF : 3;
			uint32_t : 6;
			enum ccm_cbcdr_periph_clk_sel PERIPH_CLK_SEL : 1;
			uint32_t : 1;
			uint32_t PERIPH_CLK2_PODF : 3;
			uint32_t : 2;
		};
		uint32_t r;
	} CBCDR;
	union ccm_cbcmr {
		struct {
			uint32_t : 4;
			enum ccm_cbcmr_lpspi_clk_sel LPSPI_CLK_SEL : 2;
			uint32_t : 2;
			enum ccm_cmcmr_flexspi2_clk_sel FLEXSPL2_CLK_SEL : 2; /* For IMXRT106x only. RSV for IMXRT105x */
			uint32_t : 2;
			enum ccm_cbcmr_periph_clk2_sel PERIPH_CLK2_SEL : 2;
			enum ccm_cbcmr_trace_clk_sel TRACE_CLK_SEL : 2;
			uint32_t : 2;
			enum ccm_cbcmr_pre_periph_clk_sel PRE_PERIPH_CLK_SEL : 2;
			uint32_t : 3;
			uint32_t LCDIF_PODF : 3;
			uint32_t LPSPI_PODF : 3;
			uint32_t FLEXSPI2_PODF : 3; /* For IMXRT106x only. RSV for IMXRT105x */
		};
		uint32_t r;
	} CBCMR;
	union ccm_cscmr1 {
		struct {
			uint32_t PERCLK_PODF : 6;
			enum ccm_cscmr1_perclk_clk_sel PERCLK_CLK_SEL : 1;
			uint32_t : 3;
			enum ccm_cscmr1_sai1_clk_sel SAI1_CLK_SEL : 2;
			enum ccm_cscmr1_sai2_clk_sel SAI2_CLK_SEL : 2;
			enum ccm_cscmr1_sai3_clk_sel SAI3_CLK_SEL : 2;
			enum ccm_cscmr1_usdhc1_clk_sel USDHC1_CLK_SEL : 1;
			enum ccm_cscmr1_usdhc2_clk_sel USDHC2_CLK_SEL : 1;
			uint32_t : 5;
			uint32_t FLEXSPI_PODF : 3;
			uint32_t : 3;
			enum ccm_cscmr1_flexspi_clk_sel FLEXSPI_CLK_SEL : 2;
			uint32_t : 1;
		};
		uint32_t r;
	} CSCMR1;
	union ccm_cscmr2 {
		struct {
			uint32_t : 2;
			uint32_t CAN_CLK_PODF : 6;
			enum ccm_cscmr2_can_clk_sel CAN_CLK_SEL : 2;
			uint32_t : 9;
			enum ccm_cscmr2_flexio2_clk_sel FLEXIO2_CLK_SEL : 2;
			uint32_t : 11;
		};
		uint32_t r;
	} CSCMR2;
	union ccm_cscdr1 {
		struct {
			uint32_t UART_CLK_PODF : 6;
			enum ccm_cscdr1_uart_clk_sel UART_CLK_SEL : 1;
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
	union ccm_cdhipr {
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
	union ccm_ccgr0 {
		struct {
			enum ccm_cg aips_tz1 : 2;
			enum ccm_cg aips_tz2 : 2;
			enum ccm_cg mqs : 2;
			enum ccm_cg flexspi_exsc : 2; /* For IMXRT105x only. RSV for IMXRT106x */
			enum ccm_cg sim_main : 2;
			enum ccm_cg dcp : 2;
			enum ccm_cg lpuart3 : 2;
			enum ccm_cg can1 : 2;
			enum ccm_cg can1_serial : 2;
			enum ccm_cg can2 : 2;
			enum ccm_cg can2_serial : 2;
			enum ccm_cg trace : 2;
			enum ccm_cg gpt2_bus : 2;
			enum ccm_cg gpt2_serial : 2;
			enum ccm_cg lpuart2 : 2;
			enum ccm_cg gpio2 : 2;
		};
		uint32_t r;
	} CCGR0;
	union ccm_ccgr1 {
		struct {
			enum ccm_cg lpspi1 : 2;
			enum ccm_cg lpspi2 : 2;
			enum ccm_cg lpspi3 : 2;
			enum ccm_cg lpspi4 : 2;
			enum ccm_cg adc2 : 2;
			enum ccm_cg enet : 2;
			enum ccm_cg pit : 2;
			enum ccm_cg aoi2 : 2;
			enum ccm_cg adc1 : 2;
			enum ccm_cg semc_exsc : 2;
			enum ccm_cg gpt1_bus : 2;
			enum ccm_cg gpt1_serial : 2;
			enum ccm_cg lpuart4 : 2;
			enum ccm_cg gpio1 : 2;
			enum ccm_cg csu : 2;
			enum ccm_cg gpio5: 2; /* For IMXRT106x only. RSV for IMXRT105x */
		};
		uint32_t r;
	} CCGR1;
	union ccm_ccgr2 {
		struct {
			enum ccm_cg ocram_exsc : 2;
			enum ccm_cg csi : 2;
			enum ccm_cg iomuxc_snvs : 2;
			enum ccm_cg lpi2c1 : 2;
			enum ccm_cg lpi2c2 : 2;
			enum ccm_cg lpi2c3 : 2;
			enum ccm_cg ocotp : 2; /* OCOTP for IMXRT106x. IIM for IMXRT105x. Functionally the same */
			enum ccm_cg xbar3 : 2;
			enum ccm_cg ipmux1 : 2;
			enum ccm_cg ipmux2 : 2;
			enum ccm_cg ipmux3 : 2;
			enum ccm_cg xbar1 : 2;
			enum ccm_cg xbar2 : 2;
			enum ccm_cg gpio3 : 2;
			enum ccm_cg lcd : 2;
			enum ccm_cg pxp : 2;
		};
		uint32_t r;
	} CCGR2;
	union ccm_ccgr3 {
		struct {
			enum ccm_cg flexio2 : 2;
			enum ccm_cg lpuart5 : 2;
			enum ccm_cg semc : 2;
			enum ccm_cg lpuart6 : 2;
			enum ccm_cg aoi1 : 2;
			enum ccm_cg lcdif_pix : 2;
			enum ccm_cg gpio4 : 2;
			enum ccm_cg ewm : 2;
			enum ccm_cg wdog1 : 2;
			enum ccm_cg flexram : 2;
			enum ccm_cg acmp1 : 2;
			enum ccm_cg acmp2 : 2;
			enum ccm_cg acmp3 : 2;
			enum ccm_cg acmp4 : 2;
			enum ccm_cg ocram : 2;
			enum ccm_cg iomux_snvs_gpr : 2;
		};
		uint32_t r;
	} CCGR3;
	union ccm_ccgr4 {
		struct {
			enum ccm_cg sim_m7_mainclk : 2;
			enum ccm_cg iomuxc : 2;
			enum ccm_cg iomuxc_gpr : 2;
			enum ccm_cg bee : 2;
			enum ccm_cg sim_m7 : 2;
			enum ccm_cg tsc_dig : 2;
			enum ccm_cg sim_m : 2;
			enum ccm_cg sim_ems : 2;
			enum ccm_cg pwm1 : 2;
			enum ccm_cg pwm2 : 2;
			enum ccm_cg pwm3 : 2;
			enum ccm_cg pwm4 : 2;
			enum ccm_cg enc1 : 2;
			enum ccm_cg enc2 : 2;
			enum ccm_cg enc3 : 2;
			enum ccm_cg enc4 : 2;
		};
		uint32_t r;
	} CCGR4;
	union ccm_ccgr5 {
		struct {
			enum ccm_cg rom : 2;
			enum ccm_cg flexio1 : 2;
			enum ccm_cg wdog3 : 2;
			enum ccm_cg dma : 2;
			enum ccm_cg kpp : 2;
			enum ccm_cg wdog2 : 2;
			enum ccm_cg aips_tz4 : 2;
			enum ccm_cg spdif : 2;
			enum ccm_cg sim_main : 2;
			enum ccm_cg sai1 : 2;
			enum ccm_cg sai2 : 2;
			enum ccm_cg sai3 : 2;
			enum ccm_cg lpuart1 : 2;
			enum ccm_cg lpuart7 : 2;
			enum ccm_cg snvs_hp : 2;
			enum ccm_cg snvs_lp : 2;
		};
		uint32_t r;
	} CCGR5;
	union ccm_ccgr6 {
		struct {
			enum ccm_cg usboh3 : 2;
			enum ccm_cg usdhc1 : 2;
			enum ccm_cg usdhc2 : 2;
			enum ccm_cg dcdc : 2;
			enum ccm_cg ipmux4 : 2;
			enum ccm_cg flexspi : 2;
			enum ccm_cg trng : 2;
			enum ccm_cg lpuart8 : 2;
			enum ccm_cg timer4 : 2;
			enum ccm_cg aips_tz3 : 2;
			enum ccm_cg sim_per : 2;
			enum ccm_cg anadig : 2;
			enum ccm_cg lpi2c4 : 2;
			enum ccm_cg timer1 : 2;
			enum ccm_cg timer2 : 2;
			enum ccm_cg timer3 : 2;
		};
		uint32_t r;
	} CCGR6;
	union ccm_ccgr7 {
		struct {
			enum ccm_cg enet2 : 2;
			enum ccm_cg flexspi2 : 2;
			enum ccm_cg axbs : 2;
			enum ccm_cg can3 : 2;
			enum ccm_cg can3_serial : 2;
			enum ccm_cg aips_lite : 2;
			enum ccm_cg flexio3 : 2;
			uint32_t : 18;
		};
		uint32_t r;
	} CCGR7; /* For IMXRT106x only. RSV for IMXRT105x */
	uint32_t CMEOR;
};
static_assert(sizeof(struct ccm) == 0x8c, "");

enum BYPASS_CLK_SRC {
	BYPASS_CLK_SRC_REF_CLK_24M,
	BYPASS_CLK_SRC_CLK1,
};

union ccm_analog_pll_arm {
	struct {
		uint32_t DIV_SELECT : 7;
		uint32_t : 5;
		uint32_t POWERDOWN : 1;
		uint32_t ENABLE : 1;
		enum BYPASS_CLK_SRC BYPASS_CLK_SRC : 2;
		uint32_t BYPASS : 1;
		uint32_t : 2;
		uint32_t PLL_SEL : 1;
		uint32_t : 11;
		uint32_t LOCK : 1;
	};
	uint32_t r;
};

union ccm_analog_pll_usb {
	struct {
		uint32_t : 1;
		uint32_t DIV_SELECT : 1;
		uint32_t : 4;
		uint32_t EN_USB_CLKS : 1;
		uint32_t : 5;
		uint32_t POWER : 1;
		uint32_t ENABLE : 1;
		enum BYPASS_CLK_SRC BYPASS_CLK_SRC : 2;
		uint32_t BYPASS : 1;
		uint32_t : 14;
		uint32_t LOCK : 1;
	};
	uint32_t r;
};

union ccm_analog_pll_sys {
	struct {
		uint32_t DIV_SELECT : 1;
		uint32_t : 11;
		uint32_t POWERDOWN : 1;
		uint32_t ENABLE : 1;
		enum BYPASS_CLK_SRC BYPASS_CLK_SRC : 2;
		uint32_t BYPASS : 1;
		uint32_t : 14;
		uint32_t LOCK : 1;
	};
	uint32_t r;
};

union ccm_analog_pfd {
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

struct ccm_analog {
	union ccm_analog_pll_arm PLL_ARM;
	union ccm_analog_pll_arm PLL_ARM_SET;
	union ccm_analog_pll_arm PLL_ARM_CLR;
	union ccm_analog_pll_arm PLL_ARM_TOG;
	union ccm_analog_pll_usb PLL_USB1;
	union ccm_analog_pll_usb PLL_USB1_SET;
	union ccm_analog_pll_usb PLL_USB1_CLR;
	union ccm_analog_pll_usb PLL_USB1_TOG;
	union ccm_analog_pll_usb PLL_USB2;
	union ccm_analog_pll_usb PLL_USB2_SET;
	union ccm_analog_pll_usb PLL_USB2_CLR;
	union ccm_analog_pll_usb PLL_USB2_TOG;
	union ccm_analog_pll_sys PLL_SYS;
	union ccm_analog_pll_sys PLL_SYS_SET;
	union ccm_analog_pll_sys PLL_SYS_CLR;
	union ccm_analog_pll_sys PLL_SYS_TOG;
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
	union ccm_analog_pfd PFD_480;
	union ccm_analog_pfd PFD_480_SET;
	union ccm_analog_pfd PFD_480_CLR;
	union ccm_analog_pfd PFD_480_TOG;
	union ccm_analog_pfd PFD_528;
	union ccm_analog_pfd PFD_528_SET;
	union ccm_analog_pfd PFD_528_CLR;
	union ccm_analog_pfd PFD_528_TOG;
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
static_assert(sizeof(struct ccm_analog) == 0x180, "");

#define CCM_ADDR 0x400fc000
#define CCM_ANALOG_ADDR 0x400d8000
static struct ccm *const CCM = (struct ccm*)CCM_ADDR;
static struct ccm_analog *const CCM_ANALOG = (struct ccm_analog*)CCM_ANALOG_ADDR;

#endif
