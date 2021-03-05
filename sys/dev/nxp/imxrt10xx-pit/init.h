#pragma once

/*
 * Driver for Periodic Interrupt Timer (PIT) on IMXRT10xx processors
 */

struct nxp_imxrt10xx_pit_desc {
	unsigned long base;		/* module base address */
	unsigned long clock;		/* module clock frequency */
	int irq;			/* interrupt number */
	int ipl;			/* interrupt priotity level */
};

void nxp_imxrt10xx_pit_init(const nxp_imxrt10xx_pit_desc *);
