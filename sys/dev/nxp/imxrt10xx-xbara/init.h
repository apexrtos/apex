#pragma once

/*
 * Driver for Inter-Peripheral Crossbar Switch (XBARA) on IMXRT10xx processors
 */

#ifdef __cplusplus
extern "C" {
#endif

struct nxp_imxrt10xx_xbara_desc {
	unsigned long base;		/* module base address */
};

void nxp_imxrt10xx_xbara_init(const struct nxp_imxrt10xx_xbara_desc *);

#ifdef __cplusplus
}
#endif
