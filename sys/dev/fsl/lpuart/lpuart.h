#pragma once

/*
 * Device driver for freescale LPUART generally found on imx processors
 *
 * Kernel entry points
 */

#include <stddef.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

void fsl_lpuart_early_init(unsigned long base, unsigned long clock, tcflag_t);
void fsl_lpuart_early_print(unsigned long base, const char *, size_t);

#ifdef __cplusplus
} /* extern "C" */
#endif
