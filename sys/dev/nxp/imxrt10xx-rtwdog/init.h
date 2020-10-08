#pragma once

/*
 * Driver for RT Watchdog (RTWDOG) on IMXRT10xx processors
 */

#include "imxrt10xx-rtwdog.h"

struct nxp_imxrt10xx_rtwdog_desc {
	const char *name;			/* device name */
	unsigned long base;			/* module base address */
	imxrt10xx::rtwdog::clock clock;		/* module clock source */
	unsigned long freq;			/* module clock frequency */
	bool prescale_256;		    /* enable /256 clock prescaler */
	unsigned default_timeout;	    /* default watchdog timeout */
};

void nxp_imxrt10xx_rtwdog_init(const nxp_imxrt10xx_rtwdog_desc *);
