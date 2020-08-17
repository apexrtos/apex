#pragma once

/*
 * Driver for True Random Number Generator (TRNG) Controller on IMXRT10xx processors
 */

struct nxp_imxrt10xx_trng_desc {
	const char *name;		/* device name */
	unsigned long base;		/* module base address */
	int irq;			/* interrupt number */
	int ipl;			/* interrupt priotity level */
};

void nxp_imxrt10xx_trng_init(const struct nxp_imxrt10xx_trng_desc *);
