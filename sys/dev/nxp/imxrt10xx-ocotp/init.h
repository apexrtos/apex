#pragma once

/*
 * Driver for On-Chip One Time Programmable (OCOTP) Controller on IMXRT10xx processors
 */

struct nxp_imxrt10xx_ocotp_desc {
	const char *name;		/* device name */
	unsigned long base;		/* module base address */
	unsigned long clock;		/* module clock frequency */
};

void nxp_imxrt10xx_ocotp_init(const struct nxp_imxrt10xx_ocotp_desc *);
