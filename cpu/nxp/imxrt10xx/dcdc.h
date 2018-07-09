#ifndef imxrt10xx_dcdc_h
#define imxrt10xx_dcdc_h

#include <assert.h>
#include <stdint.h>

/*
 * DCDC Converter
 */
struct dcdc {
	union dcdc_reg0 {
		struct {
			uint32_t PWD_ZCD : 1;
			uint32_t DISABLE_AUTO_CLK_SWITCH : 1;
			uint32_t SEL_CLK : 1;
			uint32_t PWD_OSC_INT : 1;
			uint32_t PWD_CUR_SNS_CMP : 1;
			uint32_t CUR_SNS_THRSH : 3;
			uint32_t PWD_OVERCUR_DET : 1;
			uint32_t OVERCUR_TRIG_ADJ : 2;
			uint32_t PWD_CMP_BATT_DET : 1;
			uint32_t ADJ_POSLIMIT_BUCK : 4;
			uint32_t EN_LP_OVERLOAD_SNS : 1;
			uint32_t PWD_HIGH_VOLT_DET : 1;
			uint32_t LP_OVERLOAD_THRSH : 2;
			uint32_t LP_OVERLOAD_FREQ_SEL : 1;
			uint32_t LP_HIGH_HYS : 1;
			uint32_t : 4;
			uint32_t PWD_CMP_OFFSET : 1;
			uint32_t XTALOK_DISABLE : 1;
			uint32_t CURRENT_ALERT_RESET : 1;
			uint32_t XTAL_25M_OK : 1;
			uint32_t : 1;
			uint32_t STS_DC_OK : 1;
		};
		uint32_t r;
	} REG0;
	union dcdc_reg1 {
		struct {
			uint32_t POSLIMIT_BUCK_IN : 7;
			uint32_t REG_FBK_SEL : 2;
			uint32_t REG_RLOAD_SW : 1;
			uint32_t : 2;
			uint32_t LPCMP_ISRC_SEL : 2;
			uint32_t NEGLIMIT_IN : 7;
			uint32_t LOOPCTRL_HYST_THRESH : 1;
			uint32_t : 1;
			uint32_t LOOPCTRL_EN_HYST : 1;
			uint32_t VBG_TRIM : 5;
			uint32_t : 3;
		};
		uint32_t r;
	} REG1;
	union dcdc_reg2 {
		struct {
			uint32_t LOOPCTRL_DC_C : 2;
			uint32_t LOOPCTRL_DC_R : 4;
			uint32_t LOOPCTRL_DC_FF : 3;
			uint32_t LOOPCTRL_EN_RCSCALE : 3;
			uint32_t LOOPCTRL_RCSCALE_THRSH : 1;
			uint32_t LOOPCTRL_HYST_SIGN : 1;
			uint32_t : 13;
			uint32_t DISABLE_PULSE_SKIP : 1;
			uint32_t DCM_SET_CTRL : 1;
			uint32_t : 3;
		};
		uint32_t r;
	} REG2;
	union dcdc_reg3 {
		struct {
			uint32_t TRG : 5;
			uint32_t : 3;
			uint32_t TARGET_LP : 3;
			uint32_t : 13;
			uint32_t MINPWR_DC_HALFCLK : 1;
			uint32_t : 2;
			uint32_t MISC_DELAY_TIMING : 1;
			uint32_t MISC_DISABLEFET_LOGIC : 1;
			uint32_t : 1;
			uint32_t DISABLE_STEP : 1;
			uint32_t : 1;
		};
		uint32_t r;
	} REG3;
};
static_assert(sizeof(struct dcdc) == 0x10, "");

static volatile struct dcdc *const DCDC = (struct dcdc*)0x40080000;

#endif
