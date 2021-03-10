#pragma once

#include <cassert>
#include <cstdint>

/*
 * Power Management Unit
 */
struct pmu {
	union reg_3p0 {
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

	uint32_t REG_1P1;
	uint32_t REG_1P1_SET;
	uint32_t REG_1P1_CLR;
	uint32_t REG_1P1_TOG;
	reg_3p0 REG_3P0;
	reg_3p0 REG_3P0_SET;
	reg_3p0 REG_3P0_CLR;
	reg_3p0 REG_3P0_TOG;
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
static_assert(sizeof(pmu) == 0x70);

static pmu *const PMU = (pmu*)0x400d8110;
