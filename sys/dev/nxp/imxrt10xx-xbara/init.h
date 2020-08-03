#pragma once

/*
 * Driver for Inter-Peripheral Crossbar Switch (XBARA) on IMXRT10xx processors
 */

#include "imxrt10xx-xbara.h"
#include <initializer_list>
#include <utility>

#ifdef __cplusplus
extern "C" {
#endif

struct nxp_imxrt10xx_xbara_desc {
	using connection = std::pair<imxrt10xx::xbara::output, imxrt10xx::xbara::input>;

	unsigned long base;			    /* module base address */
	std::initializer_list<connection> config;   /* startup configuration */
};

void nxp_imxrt10xx_xbara_init(const struct nxp_imxrt10xx_xbara_desc *);

#ifdef __cplusplus
}
#endif
