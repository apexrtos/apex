#pragma once

/*
 * Device driver for freescale LPUART generally found on imx processors
 */

#ifdef __cplusplus
extern "C" {
#endif

struct fsl_lpuart_desc {
	const char *name;
	unsigned long base;
	unsigned long clock;
	int ipl;
	int rx_tx_int;
};

void fsl_lpuart_init(const struct fsl_lpuart_desc *);

#ifdef __cplusplus
} /* extern "C" */
#endif
