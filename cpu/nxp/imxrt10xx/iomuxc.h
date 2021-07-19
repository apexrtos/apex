#pragma once

#include "bitfield.h"
#include <cassert>
#include <cstdint>

/*
 * IOMUX Controller
 */
struct iomuxc {
	union sw_mux_ctl {
		using S = uint32_t;
		struct { S r; };
		enum class sion {
			Software_Input_On_Disabled,
			Software_Input_On_Enabled,
		};
		bitfield::imxbit<S, sion, 4> SION;
		bitfield::imxbits<S, unsigned, 2, 0> MUX_MODE;

		constexpr
		sw_mux_ctl()
		: r{}
		{}

		constexpr
		sw_mux_ctl(unsigned mux_mode, sion sion)
		: r{decltype(MUX_MODE)(mux_mode).r | decltype(SION)(sion).r}
		{}
	};

	union sw_pad_ctl {
		using S = uint32_t;
		struct { S r; };
		enum class hys {
			Hysteresis_Disabled,
			Hysteresis_Enabled,
		};
		bitfield::imxbit<S, hys, 16> HYS;
		enum class pus {
			Pull_Down_100K,
			Pull_Up_47K,
			Pull_Up_100K,
			Pull_Up_22K,
		};
		bitfield::imxbits<S, pus, 15, 14> PUS;
		enum class pue {
			Keeper,
			Pull,
		};
		bitfield::imxbit<S, pue, 13> PUE;
		enum class pke {
			Pull_Keeper_Disabled,
			Pull_Keeper_Enabled,
		};
		bitfield::imxbit<S, pke, 12> PKE;
		enum class ode {
			Open_Drain_Disabled,
			Open_Drain_Enabled,
		};
		bitfield::imxbit<S, ode, 11> ODE;
		enum class speed {
			MHz_50 = 0,
			MHz_100 = 2,
			MHz_200 = 3,
		};
		bitfield::imxbits<S, speed, 7, 6> SPEED;
		enum class dse {
			disabled,
			R0,
			R0_2,
			R0_3,
			R0_4,
			R0_5,
			R0_6,
			R0_7,
		};
		bitfield::imxbits<S, dse, 5, 3> DSE;
		enum class sre {
			Slow,
			Fast,
		};
		bitfield::imxbit<S, sre, 0> SRE;

		constexpr
		sw_pad_ctl()
		: r{}
		{}

		constexpr
		sw_pad_ctl(sre sre, dse dse, speed speed, ode ode, pke pke,
			   pue pue, pus pus, hys hys)
		: r{decltype(SRE)(sre).r | decltype(DSE)(dse).r |
		    decltype(SPEED)(speed).r | decltype(ODE)(ode).r |
		    decltype(PKE)(pke).r | decltype(PUE)(pue).r |
		    decltype(PUS)(pus).r | decltype(HYS)(hys).r}
		{}

		static constexpr sw_pad_ctl
		out_push_pull(sre sre, dse dse, speed speed)
		{
			return {sre, dse, speed,
				ode::Open_Drain_Disabled,
				pke::Pull_Keeper_Disabled,
				pue::Keeper,		/* disabled */
				pus::Pull_Down_100K,	/* disabled */
				hys::Hysteresis_Enabled};
		}

		static constexpr sw_pad_ctl
		out_open_drain(sre sre, dse dse, speed speed)
		{
			return {sre, dse, speed,
				ode::Open_Drain_Enabled,
				pke::Pull_Keeper_Disabled,
				pue::Keeper,		/* disabled */
				pus::Pull_Down_100K,	/* disabled */
				hys::Hysteresis_Enabled};
		}

		static constexpr sw_pad_ctl
		in_digital()
		{
			return {sre::Slow, dse::disabled, speed::MHz_50,
				ode::Open_Drain_Disabled,
				pke::Pull_Keeper_Disabled,
				pue::Keeper,		/* disabled */
				pus::Pull_Down_100K,	/* disabled */
				hys::Hysteresis_Enabled};
		}

		static constexpr sw_pad_ctl
		in_analog()
		{
			return {sre::Slow, dse::disabled, speed::MHz_50,
				ode::Open_Drain_Disabled,
				pke::Pull_Keeper_Disabled,
				pue::Keeper,		/* disabled */
				pus::Pull_Down_100K,	/* disabled */
				hys::Hysteresis_Disabled};
		}
	};

	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	union {
		sw_mux_ctl SW_MUX_CTL[124];
		struct {
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_EMC[42];
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_AD_B0[16];
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_AD_B1[16];
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_B0[16];
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_B1[16];
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_SD_B0[6];
			sw_mux_ctl SW_MUX_CTL_PAD_GPIO_SD_B1[12];
		};
	};
	union {
		sw_pad_ctl SW_PAD_CTL[124];
		struct {
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_EMC[42];
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_AD_B0[16];
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_AD_B1[16];
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_B0[16];
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_B1[16];
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_SD_B0[6];
			sw_pad_ctl SW_PAD_CTL_PAD_GPIO_SD_B1[12];
		};
	};
	uint32_t ANATOP_USB_OTG1_ID_SELECT_INPUT;
	uint32_t ANATOP_USB_OTG2_ID_SELECT_INPUT;
	uint32_t CCM_PMIC_READY_SELECT_INPUT;
	uint32_t CSI_DATA02_SELECT_INPUT;
	uint32_t CSI_DATA03_SELECT_INPUT;
	uint32_t CSI_DATA04_SELECT_INPUT;
	uint32_t CSI_DATA05_SELECT_INPUT;
	uint32_t CSI_DATA06_SELECT_INPUT;
	uint32_t CSI_DATA07_SELECT_INPUT;
	uint32_t CSI_DATA08_SELECT_INPUT;
	uint32_t CSI_DATA09_SELECT_INPUT;
	uint32_t CSI_HSYNC_SELECT_INPUT;
	uint32_t CSI_PIXCLK_SELECT_INPUT;
	uint32_t CSI_VSYNC_SELECT_INPUT;
	uint32_t ENET_IPG_CLK_RMII_SELECT_INPUT;
	uint32_t ENET_MDIO_SELECT_INPUT;
	uint32_t ENET0_RXDATA_SELECT_INPUT;
	uint32_t ENET1_RXDATA_SELECT_INPUT;
	uint32_t ENET_RXEN_SELECT_INPUT;
	uint32_t ENET_RXERR_SELECT_INPUT;
	uint32_t ENET0_TIMER_SELECT_INPUT;
	uint32_t ENET_TXCLK_SELECT_INPUT;
	uint32_t FLEXCAN1_RX_SELECT_INPUT;
	uint32_t FLEXCAN2_RX_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMA3_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMA0_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMA1_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMA2_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMB3_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMB0_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMB1_SELECT_INPUT;
	uint32_t FLEXPWM1_PWMB2_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMA3_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMA0_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMA1_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMA2_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMB3_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMB0_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMB1_SELECT_INPUT;
	uint32_t FLEXPWM2_PWMB2_SELECT_INPUT;
	uint32_t FLEXPWM4_PWMA0_SELECT_INPUT;
	uint32_t FLEXPWM4_PWMA1_SELECT_INPUT;
	uint32_t FLEXPWM4_PWMA2_SELECT_INPUT;
	uint32_t FLEXPWM4_PWMA3_SELECT_INPUT;
	uint32_t FLEXSPIA_DQS_SELECT_INPUT;
	uint32_t FLEXSPIA_DATA0_SELECT_INPUT;
	uint32_t FLEXSPIA_DATA1_SELECT_INPUT;
	uint32_t FLEXSPIA_DATA2_SELECT_INPUT;
	uint32_t FLEXSPIA_DATA3_SELECT_INPUT;
	uint32_t FLEXSPIB_DATA0_SELECT_INPUT;
	uint32_t FLEXSPIB_DATA1_SELECT_INPUT;
	uint32_t FLEXSPIB_DATA2_SELECT_INPUT;
	uint32_t FLEXSPIB_DATA3_SELECT_INPUT;
	uint32_t FLEXSPIA_SCK_SELECT_INPUT;
	uint32_t LPI2C1_SCL_SELECT_INPUT;
	uint32_t LPI2C1_SDA_SELECT_INPUT;
	uint32_t LPI2C2_SCL_SELECT_INPUT;
	uint32_t LPI2C2_SDA_SELECT_INPUT;
	uint32_t LPI2C3_SCL_SELECT_INPUT;
	uint32_t LPI2C3_SDA_SELECT_INPUT;
	uint32_t LPI2C4_SCL_SELECT_INPUT;
	uint32_t LPI2C4_SDA_SELECT_INPUT;
	uint32_t LPSPI1_PCS0_SELECT_INPUT;
	uint32_t LPSPI1_SCK_SELECT_INPUT;
	uint32_t LPSPI1_SDI_SELECT_INPUT;
	uint32_t LPSPI1_SDO_SELECT_INPUT;
	uint32_t LPSPI2_PCS0_SELECT_INPUT;
	uint32_t LPSPI2_SCK_SELECT_INPUT;
	uint32_t LPSPI2_SDI_SELECT_INPUT;
	uint32_t LPSPI2_SDO_SELECT_INPUT;
	uint32_t LPSPI3_PCS0_SELECT_INPUT;
	uint32_t LPSPI3_SCK_SELECT_INPUT;
	uint32_t LPSPI3_SDI_SELECT_INPUT;
	uint32_t LPSPI3_SDO_SELECT_INPUT;
	uint32_t LPSPI4_PCS0_SELECT_INPUT;
	uint32_t LPSPI4_SCK_SELECT_INPUT;
	uint32_t LPSPI4_SDI_SELECT_INPUT;
	uint32_t LPSPI4_SDO_SELECT_INPUT;
	uint32_t LPUART2_RX_SELECT_INPUT;
	uint32_t LPUART2_TX_SELECT_INPUT;
	uint32_t LPUART3_CTS_B_SELECT_INPUT;
	uint32_t LPUART3_RX_SELECT_INPUT;
	uint32_t LPUART3_TX_SELECT_INPUT;
	uint32_t LPUART4_RX_SELECT_INPUT;
	uint32_t LPUART4_TX_SELECT_INPUT;
	uint32_t LPUART5_RX_SELECT_INPUT;
	uint32_t LPUART5_TX_SELECT_INPUT;
	uint32_t LPUART6_RX_SELECT_INPUT;
	uint32_t LPUART6_TX_SELECT_INPUT;
	uint32_t LPUART7_RX_SELECT_INPUT;
	uint32_t LPUART7_TX_SELECT_INPUT;
	uint32_t LPUART8_RX_SELECT_INPUT;
	uint32_t LPUART8_TX_SELECT_INPUT;
	uint32_t NMI_SELECT_INPUT;
	uint32_t QTIMER2_TIMER0_SELECT_INPUT;
	uint32_t QTIMER2_TIMER1_SELECT_INPUT;
	uint32_t QTIMER2_TIMER2_SELECT_INPUT;
	uint32_t QTIMER2_TIMER3_SELECT_INPUT;
	uint32_t QTIMER3_TIMER0_SELECT_INPUT;
	uint32_t QTIMER3_TIMER1_SELECT_INPUT;
	uint32_t QTIMER3_TIMER2_SELECT_INPUT;
	uint32_t QTIMER3_TIMER3_SELECT_INPUT;
	uint32_t SAI1_MCLK2_SELECT_INPUT;
	uint32_t SAI1_RX_BCLK_SELECT_INPUT;
	uint32_t SAI1_RX_DATA0_SELECT_INPUT;
	uint32_t SAI1_RX_DATA1_SELECT_INPUT;
	uint32_t SAI1_RX_DATA2_SELECT_INPUT;
	uint32_t SAI1_RX_DATA3_SELECT_INPUT;
	uint32_t SAI1_RX_SYNC_SELECT_INPUT;
	uint32_t SAI1_TX_BCLK_SELECT_INPUT;
	uint32_t SAI1_TX_SYNC_SELECT_INPUT;
	uint32_t SAI2_MCLK2_SELECT_INPUT;
	uint32_t SAI2_RX_BCLK_SELECT_INPUT;
	uint32_t SAI2_RX_DATA0_SELECT_INPUT;
	uint32_t SAI2_RX_SYNC_SELECT_INPUT;
	uint32_t SAI2_TX_BCLK_SELECT_INPUT;
	uint32_t SAI2_TX_SYNC_SELECT_INPUT;
	uint32_t SPDIF_IN_SELECT_INPUT;
	uint32_t USB_OTG2_OC_SELECT_INPUT;
	uint32_t USB_OTG1_OC_SELECT_INPUT;
	uint32_t USDHC1_CD_B_SELECT_INPUT;
	uint32_t USDHC1_WP_SELECT_INPUT;
	uint32_t USDHC2_CLK_SELECT_INPUT;
	uint32_t USDHC2_CD_B_SELECT_INPUT;
	uint32_t USDHC2_CMD_SELECT_INPUT;
	uint32_t USDHC2_DATA0_SELECT_INPUT;
	uint32_t USDHC2_DATA1_SELECT_INPUT;
	uint32_t USDHC2_DATA2_SELECT_INPUT;
	uint32_t USDHC2_DATA3_SELECT_INPUT;
	uint32_t USDHC2_DATA4_SELECT_INPUT;
	uint32_t USDHC2_DATA5_SELECT_INPUT;
	uint32_t USDHC2_DATA6_SELECT_INPUT;
	uint32_t USDHC2_DATA7_SELECT_INPUT;
	uint32_t USDHC2_WP_SELECT_INPUT;
	uint32_t XBAR1_IN02_SELECT_INPUT;
	uint32_t XBAR1_IN03_SELECT_INPUT;
	uint32_t XBAR1_IN04_SELECT_INPUT;
	uint32_t XBAR1_IN05_SELECT_INPUT;
	uint32_t XBAR1_IN06_SELECT_INPUT;
	uint32_t XBAR1_IN07_SELECT_INPUT;
	uint32_t XBAR1_IN08_SELECT_INPUT;
	uint32_t XBAR1_IN09_SELECT_INPUT;
	uint32_t XBAR1_IN17_SELECT_INPUT;
	uint32_t XBAR1_IN18_SELECT_INPUT;
	uint32_t XBAR1_IN20_SELECT_INPUT;
	uint32_t XBAR1_IN22_SELECT_INPUT;
	uint32_t XBAR1_IN23_SELECT_INPUT;
	uint32_t XBAR1_IN24_SELECT_INPUT;
	uint32_t XBAR1_IN14_SELECT_INPUT;
	uint32_t XBAR1_IN15_SELECT_INPUT;
	uint32_t XBAR1_IN16_SELECT_INPUT;
	uint32_t XBAR1_IN25_SELECT_INPUT;
	uint32_t XBAR1_IN19_SELECT_INPUT;
	uint32_t XBAR1_IN21_SELECT_INPUT;
	/* Below - For IMXRT106x only. RSV for IMXRT105x */
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
	uint32_t ENET2_IPG_CLK_RMII_SELECT_INPUT;
	uint32_t ENET2_IPP_IND_MAC0_MDIO_SELECT_INPUT;
	uint32_t ENET2_IPP_IND_MAC0_RXDATA_SELECT_INPUT_0;
	uint32_t ENET2_IPP_IND_MAC0_RXDATA_SELECT_INPUT_1;
	uint32_t ENET2_IPP_IND_MAC0_RXEN_SELECT_INPUT;
	uint32_t ENET2_IPP_IND_MAC0_RXERR_SELECT_INPUT;
	uint32_t ENET2_IPP_IND_MAC0_TIMER_SELECT_INPUT_0;
	uint32_t ENET2_IPP_IND_MAC0_TXCLK_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_DQS_FA_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FA_BIT0_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FA_BIT1_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FA_BIT2_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FA_BIT3_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FB_BIT0_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FB_BIT1_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FB_BIT2_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_IO_FB_BIT3_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_SCK_FA_SELECT_INPUT;
	uint32_t FLEXSPI2_IPP_IND_SCK_FB_SELECT_INPUT;
	uint32_t GPT1_IPP_IND_CAPIN1_SELECT_INPUT;
	uint32_t GPT1_IPP_IND_CAPIN2_SELECT_INPUT;
	uint32_t GPT1_IPP_IND_CLKIN_SELECT_INPUT;
	uint32_t GPT2_IPP_IND_CAPIN1_SELECT_INPUT;
	uint32_t GPT2_IPP_IND_CAPIN2_SELECT_INPUT;
	uint32_t GPT2_IPP_IND_CLKIN_SELECT_INPUT;
	uint32_t SAI3_IPG_CLK_SAI_MCLK_SELECT_INPUT_2;
	uint32_t SAI3_IPP_IND_SAI_RXBCLK_SELECT_INPUT;
	uint32_t SAI3_IPP_IND_SAI_RXDATA_SELECT_INPUT_0;
	uint32_t SAI3_IPP_IND_SAI_RXSYNC_SELECT_INPUT;
	uint32_t SAI3_IPP_IND_SAI_TXBCLK_SELECT_INPUT;
	uint32_t SAI3_IPP_IND_SAI_TXSYNC_SELECT_INPUT;
	uint32_t SEMC_I_IPP_IND_DQS4_SELECT_INPUT;
	uint32_t CANFD_IPP_IND_CANRX_SELECT_INPUT;
};
static_assert(sizeof(iomuxc) == 0x790);

struct iomuxc_gpr {
	union gpr16 {
		using S = uint32_t;
		struct { S r; };
		bitfield::imxbits<S, unsigned, 31, 7> CM7_INIT_VTOR;
		enum class flexram_bank_cfg_sel {
			fuses,
			FLEXRAM_BANK_CFG,
		};
		bitfield::imxbit<S, flexram_bank_cfg_sel, 2> FLEXRAM_BANK_CFG_SEL;
		bitfield::imxbit<S, bool, 1> INIT_DTCM_EN;
		bitfield::imxbit<S, bool, 0> INIT_ITCM_EN;
	};

	union gpr17 {
		uint32_t FLEXRAM_BANK_CFG;
	};

	uint32_t GPR0;
	uint32_t GPR1;
	uint32_t GPR2;
	uint32_t GPR3;
	uint32_t GPR4;
	uint32_t GPR5;
	uint32_t GPR6;
	uint32_t GPR7;
	uint32_t GPR8;
	uint32_t GPR9;
	uint32_t GPR10;
	uint32_t GPR11;
	uint32_t GPR12;
	uint32_t GPR13;
	uint32_t GPR14;
	uint32_t GPR15;
	gpr16 GPR16;
	gpr17 GPR17;
	uint32_t GPR18;
	uint32_t GPR19;
	uint32_t GPR20;
	uint32_t GPR21;
	uint32_t GPR22;
	uint32_t GPR23;
	uint32_t GPR24;
	uint32_t GPR25;
};
static_assert(sizeof(iomuxc_gpr) == 0x68);

struct iomuxc_snvs {
	uint32_t SW_MUX_CTL_PAD_WAKEUP;
	uint32_t SW_MUX_CTL_PAD_PMIC_ON_REQ;
	uint32_t SW_MUX_CTL_PAD_PMIC_STBY_REQ;
	uint32_t SW_PAD_CTL_PAD_TEST_MODE;
	uint32_t SW_PAD_CTL_PAD_POR_B;
	uint32_t SW_PAD_CTL_PAD_ONOFF;
	uint32_t SW_PAD_CTL_PAD_WAKEUP;
	uint32_t SW_PAD_CTL_PAD_PMIC_ON_REQ;
	uint32_t SW_PAD_CTL_PAD_PMIC_STBY_REQ;
};
static_assert(sizeof(iomuxc_snvs) == 0x24);

struct iomuxc_snvs_gpr {
	uint32_t GPR[4];
};
static_assert(sizeof(iomuxc_snvs_gpr) == 0x10);

#define IOMUXC_ADDR 0x401f8000
#define IOMUXC_GPR_ADDR 0x400ac000
#define IOMUXC_SNVS_ADDR 0x400a8000
#define IOMUXC_SNVS_GPR_ADDR 0x400a4000
static iomuxc *const IOMUXC = (iomuxc*)IOMUXC_ADDR;
static iomuxc_gpr *const IOMUXC_GPR = (iomuxc_gpr*)IOMUXC_GPR_ADDR;
static iomuxc_snvs *const IOMUXC_SNVS = (iomuxc_snvs*)IOMUXC_SNVS_ADDR;
static iomuxc_snvs_gpr *const IOMUXC_SNVS_GPR = (iomuxc_snvs_gpr*)IOMUXC_SNVS_GPR_ADDR;
