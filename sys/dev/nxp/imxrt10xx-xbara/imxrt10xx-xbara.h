#pragma once

struct nxp_imxrt10xx_xbara_desc;
void nxp_imxrt10xx_xbara_init(const nxp_imxrt10xx_xbara_desc *);

namespace imxrt10xx {

/*
 * Driver for Inter-Peripheral Crossbar Switch A (XBARA) on IMXRT10xx processors
 */
class xbara {
public:
	enum class input {
		logic_low,
		logic_high,
		iomux_xbar_in02,
		iomux_xbar_in03,
		iomux_xbar_inout04,
		iomux_xbar_inout05,
		iomux_xbar_inout06,
		iomux_xbar_inout07,
		iomux_xbar_inout08,
		iomux_xbar_inout09,
		iomux_xbar_inout10,
		iomux_xbar_inout11,
		iomux_xbar_inout12,
		iomux_xbar_inout13,
		iomux_xbar_inout14,
		iomux_xbar_inout15,
		iomux_xbar_inout16,
		iomux_xbar_inout17,
		iomux_xbar_inout18,
		iomux_xbar_inout19,
		iomux_xbar_in20,
		iomux_xbar_in21,
		iomux_xbar_in22,
		iomux_xbar_in23,
		iomux_xbar_in24,
		iomux_xbar_in25,
		acmp1_out,
		acmp2_out,
		acmp3_out,
		acmp4_out,
		qtimer3_timer0 = 32,
		qtimer3_timer1,
		qtimer3_timer2,
		qtimer3_timer3,
		qtimer4_timer0,
		qtimer4_timer1,
		qtimer4_timer2,
		qtimer4_timer3,
		flexpwm1_pwm1_out_trig01,
		flexpwm1_pwm2_out_trig01,
		flexpwm1_pwm3_out_trig01,
		flexpwm1_pwm4_out_trig01,
		flexpwm2_pwm1_out_trig01,
		flexpwm2_pwm2_out_trig01,
		flexpwm2_pwm3_out_trig01,
		flexpwm2_pwm4_out_trig01,
		flexpwm3_pwm1_out_trig01,
		flexpwm3_pwm2_out_trig01,
		flexpwm3_pwm3_out_trig01,
		flexpwm3_pwm4_out_trig01,
		flexpwm4_pwm1_out_trig01,
		flexpwm4_pwm2_out_trig01,
		flexpwm4_pwm3_out_trig01,
		flexpwm4_pwm4_out_trig01,
		pit_trigger0,
		pit_trigger1,
		pit_trigger2,
		pit_trigger3,
		enc1_pos_match,
		enc2_pos_match,
		enc3_pos_match,
		enc4_pos_match,
		dma_done0,
		dma_done1,
		dma_done2,
		dma_done3,
		dma_done4,
		dma_done5,
		dma_done6,
		dma_done7,
		aoi1_out0,
		aoi1_out1,
		aoi1_out2,
		aoi1_out3,
		aoi2_out0,
		aoi2_out1,
		aoi2_out2,
		aoi2_out3,
		adc_etc0_coco0,
		adc_etc0_coco1,
		adc_etc0_coco2,
		adc_etc0_coco3,
		adc_etc1_coco0,
		adc_etc1_coco1,
		adc_etc1_coco2,
		adc_etc1_coco3,
	};

	enum class output {
		dma_ch_mux_reg30,
		dma_ch_mux_reg31,
		dma_ch_mux_reg94,
		dma_ch_mux_reg95,
		iomux_xbar_inout04,
		iomux_xbar_inout05,
		iomux_xbar_inout06,
		iomux_xbar_inout07,
		iomux_xbar_inout08,
		iomux_xbar_inout09,
		iomux_xbar_inout10,
		iomux_xbar_inout11,
		iomux_xbar_inout12,
		iomux_xbar_inout13,
		iomux_xbar_inout14,
		iomux_xbar_inout15,
		iomux_xbar_inout16,
		iomux_xbar_inout17,
		iomux_xbar_inout18,
		iomux_xbar_inout19,
		acmp1_sample,
		acmp2_sample,
		acmp3_sample,
		acmp4_sample,
		flexpwm1_pwm0_exta = 26,
		flexpwm1_pwm1_exta,
		flexpwm1_pwm2_exta,
		flexpwm1_pwm3_exta,
		flexpwm1_pwm0_ext_sync,
		flexpwm1_pwm1_ext_sync,
		flexpwm1_pwm2_ext_sync,
		flexpwm1_pwm3_ext_sync,
		flexpwm1_ext_clk,
		flexpwm1_fault0,
		flexpwm1_fault1,
		flexpwm1234_fault2,
		flexpwm1234_fault3,
		flexpwm1_ext_force,
		flexpwm234_pwm0_exta,
		flexpwm234_pwm1_exta,
		flexpwm234_pwm2_exta,
		flexpwm234_pwm3_exta,
		flexpwm2_pwm0_ext_sync,
		flexpwm2_pwm1_ext_sync,
		flexpwm2_pwm2_ext_sync,
		flexpwm2_pwm3_ext_sync,
		flexpwm234_ext_clk,
		flexpwm2_fault0,
		flexpwm2_fault1,
		flexpwm2_ext_force,
		flexpwm3_ext_sync0,
		flexpwm3_ext_sync1,
		flexpwm3_ext_sync2,
		flexpwm3_ext_sync3,
		flexpwm3_fault0,
		flexpwm3_fault1,
		flexpwm3_ext_force,
		flexpwm4_ext_sync0,
		flexpwm4_ext_sync1,
		flexpwm4_ext_sync2,
		flexpwm4_ext_sync3,
		flexpwm4_fault0,
		flexpwm4_fault1,
		flexpwm4_ext_force,
		enc1_phasea_input,
		enc1_phaseb_input,
		enc1_index,
		enc1_home,
		enc1_trigger,
		enc2_phasea_input,
		enc2_phaseb_input,
		enc2_index,
		enc2_home,
		enc2_trigger,
		enc3_phasea_input,
		enc3_phaseb_input,
		enc3_index,
		enc3_home,
		enc3_trigger,
		enc4_phasea_input,
		enc4_phaseb_input,
		enc4_index,
		enc4_home,
		enc4_trigger,
		qtimer1_timer0,
		qtimer1_timer1,
		qtimer1_timer2,
		qtimer1_timer3,
		qtimer2_timer0,
		qtimer2_timer1,
		qtimer2_timer2,
		qtimer2_timer3,
		qtimer3_timer0,
		qtimer3_timer1,
		qtimer3_timer2,
		qtimer3_timer3,
		qtimer4_timer0,
		qtimer4_timer1,
		qtimer4_timer2,
		qtimer4_timer3,
		ewm_ewm_in,
		adc_etc_trig_xbar0_trig0,
		adc_etc_trig_xbar0_trig1,
		adc_etc_trig_xbar0_trig2,
		adc_etc_trig_xbar0_trig3,
		adc_etc_trig_xbar1_trig0,
		adc_etc_trig_xbar1_trig1,
		adc_etc_trig_xbar1_trig2,
		adc_etc_trig_xbar1_trig3,
		lpi2c1_trg_input,
		lpi2c2_trg_input,
		lpi2c3_trg_input,
		lpi2c4_trg_input,
		lpspi1_trg_input,
		lpspi2_trg_input,
		lpspi3_trg_input,
		lpspi4_trg_input,
		lpuart1_trg_input,
		lpuart2_trg_input,
		lpuart3_trg_input,
		lpuart4_trg_input,
		lpuart5_trg_input,
		lpuart6_trg_input,
		lpuart7_trg_input,
		lpuart8_trg_input,
		flexio1_trigger_in0,
		flexio1_trigger_in1,
		flexio2_trigger_in0,
		flexio2_trigger_in1,
	};

	void set_connection(output, input);
	static xbara *inst();

private:
	struct regs;
	xbara(const nxp_imxrt10xx_xbara_desc *);
	xbara(xbara &&) = delete;
	xbara(const xbara &) = delete;
	xbara &operator=(xbara &&) = delete;
	xbara &operator=(const xbara &) = delete;

	regs *const r_;

	friend void ::nxp_imxrt10xx_xbara_init(const nxp_imxrt10xx_xbara_desc *);
};

}
