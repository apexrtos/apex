#pragma once

/*
 * Driver for On-Chip One Time Programmable (OCOTP) Controller on IMXRT10xx processors
 */

#ifdef __cplusplus
extern "C" {
#endif

struct nxp_imxrt10xx_ocotp_desc {
	const char *name;		/* device name */
	unsigned long base;		/* module base address */
	unsigned long clock;		/* module clock frequency */
};

void nxp_imxrt10xx_ocotp_init(const struct nxp_imxrt10xx_ocotp_desc *);

#ifdef __cplusplus
}
#endif
