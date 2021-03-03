#pragma once

/*
 * Driver for Freescale Ultra Secured Digital Host Controller
 */

#include <dev/mmc/desc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fsl_usdhc_desc {
	struct mmc_desc mmc;	    /* mmc host controller descriptor */
	unsigned long base;	    /* module base address */
	unsigned long clock;	    /* module clock frequency */
	int irq;		    /* interrupt number */
	int ipl;		    /* interrupt priotity level */
};

void fsl_usdhc_init(const struct fsl_usdhc_desc *);

#ifdef __cplusplus
}
#endif
