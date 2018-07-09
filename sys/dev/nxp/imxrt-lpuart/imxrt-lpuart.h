#ifndef dev_nxp_imxrt_lpuart_h
#define dev_nxp_imxrt_lpuart_h

/*
 * Device driver for LPUART found on imxrt processors
 */

#include <stddef.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

struct imxrt_lpuart_desc {
	const char *name;
	unsigned long base;
	unsigned long clock;
	int ipl;
	int rx_tx_int;
};

void imxrt_lpuart_early_init(unsigned long base, unsigned long clock, tcflag_t);
void imxrt_lpuart_early_print(unsigned long base, const char *, size_t);
void imxrt_lpuart_init(const struct imxrt_lpuart_desc *);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
