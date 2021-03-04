#pragma once

/*
 * Device driver for UART on ARM MPS2 board
 *
 * Kernel entry points
 */

#include <stddef.h>
#include <termios.h>

void arm_mps2_uart_early_init(unsigned long base, tcflag_t);
void arm_mps2_uart_early_print(unsigned long base, const char *, size_t);
