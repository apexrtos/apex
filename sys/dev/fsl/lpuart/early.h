#pragma once

/*
 * Device driver for freescale LPUART generally found on imx processors
 *
 * Kernel & bootloader entry points
 */

#include <cstddef>
#include <termios.h>

namespace fsl {

void lpuart_early_init(unsigned long base, unsigned long clock, tcflag_t);
void lpuart_early_print(unsigned long base, const char *, size_t);

}
