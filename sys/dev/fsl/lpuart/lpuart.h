#ifndef dev_fsl_lpuart_h
#define dev_fsl_lpuart_h

/*
 * Device driver for freescale LPUART generally found on imx processors
 */

#include <stddef.h>
#include <termios.h>

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

void fsl_lpuart_early_init(unsigned long base, unsigned long clock, tcflag_t);
void fsl_lpuart_early_print(unsigned long base, const char *, size_t);
void fsl_lpuart_init(const struct fsl_lpuart_desc *);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
