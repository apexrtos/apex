#pragma once

/*
 * Driver for Periodic Interrupt Timer (PIT) on IMXRT10xx processors
 */

#ifdef __cplusplus
extern "C" {
#endif

struct nxp_imxrt10xx_pit_desc {
	unsigned long base;		/* module base address */
	unsigned long clock;		/* module clock frequency */
	int irq;			/* interrupt number */
	int ipl;			/* interrupt priotity level */
};

void nxp_imxrt10xx_pit_init(const struct nxp_imxrt10xx_pit_desc *);

#ifdef __cplusplus
}
#endif
