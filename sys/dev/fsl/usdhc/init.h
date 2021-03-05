#pragma once

/*
 * Driver for Freescale Ultra Secured Digital Host Controller
 */

#include <dev/mmc/desc.h>

struct fsl_usdhc_desc {
	mmc_desc mmc;		    /* mmc host controller descriptor */
	unsigned long base;	    /* module base address */
	unsigned long clock;	    /* module clock frequency */
	int irq;		    /* interrupt number */
	int ipl;		    /* interrupt priotity level */
};

void fsl_usdhc_init(const fsl_usdhc_desc *);
