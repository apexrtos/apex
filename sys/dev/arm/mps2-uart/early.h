#pragma once

/*
 * Device driver for UART on ARM MPS2 board
 *
 * Kernel & bootloader entry points
 */

#include <cstddef>
#include <termios.h>

namespace arm {

void mps2_uart_early_init(unsigned long base, tcflag_t);
void mps2_uart_early_print(unsigned long base, const char *, size_t);

}
