#ifndef imxrt10xx_pmu_h
#define imxrt10xx_pmu_h

#include <assert.h>
#include <stdint.h>

/*
 * Power Management Unit
 */
union pmu_reg_3p0 {
	struct {
		uint32_t ENABLE_LINREG : 1;
		uint32_t ENABLE_BO : 1;
		uint32_t ENABLE_ILIMIT : 1;
		uint32_t : 1;
		uint32_t BO_OFFSET : 3;
		uint32_t VBUS_SEL : 1;
		uint32_t OUTPUT_TRG : 5;
		uint32_t : 3;
		uint32_t BO_VDD3P0 : 1;
		uint32_t OK_VDD3P0 : 1;
		uint32_t : 14;
	};
	uint32_t r;
};
struct pmu {
	uint32_t REG_1P1;
	uint32_t REG_1P1_SET;
	uint32_t REG_1P1_CLR;
	uint32_t REG_1P1_TOG;
	union pmu_reg_3p0 REG_3P0;
	union pmu_reg_3p0 REG_3P0_SET;
	union pmu_reg_3p0 REG_3P0_CLR;
	union pmu_reg_3p0 REG_3P0_TOG;
	uint32_t REG_2P5;
	uint32_t REG_2P5_SET;
	uint32_t REG_2P5_CLR;
	uint32_t REG_2P5_TOG;
	uint32_t REG_CORE;
	uint32_t REG_CORE_SET;
	uint32_t REG_CORE_CLR;
	uint32_t REG_CORE_TOG;
	uint32_t REG_MISC0;
	uint32_t REG_MISC0_SET;
	uint32_t REG_MISC0_CLR;
	uint32_t REG_MISC0_TOG;
	uint32_t REG_MISC1;
	uint32_t REG_MISC1_SET;
	uint32_t REG_MISC1_CLR;
	uint32_t REG_MISC1_TOG;
	uint32_t REG_MISC2;
	uint32_t REG_MISC2_SET;
	uint32_t REG_MISC2_CLR;
	uint32_t REG_MISC2_TOG;
};
static_assert(sizeof(struct pmu) == 0x70, "");

static volatile struct pmu *const PMU = (struct pmu*)0x400d8110;

#endif
