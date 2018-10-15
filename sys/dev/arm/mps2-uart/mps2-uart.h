#ifndef dev_arm_mps2_uart_h
#define dev_arm_mps2_uart_h

/*
 * Device driver for UART on ARM MPS2 board
 *
 * Kernel entry points
 */

#include <stddef.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

void arm_mps2_uart_early_init(unsigned long base, tcflag_t);
void arm_mps2_uart_early_print(unsigned long base, const char *, size_t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* dev_arm_mps2_uart_h */
