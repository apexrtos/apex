#ifndef dev_arm_mps2_uart_h
#define dev_arm_mps2_uart_h

/*
 * Device driver for UART on ARM MPS2 board
 *
 * Note: This driver has only been tested with QEMU and will not currently work
 * with real hardware.
 */

#include <stddef.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

struct arm_mps2_uart_desc {
	const char *name;
	unsigned long base;
	int ipl;
	int rx_int;
	int tx_int;
	int overflow_int;
};

void arm_mps2_uart_early_init(unsigned long base, tcflag_t);
void arm_mps2_uart_early_print(unsigned long base, const char *, size_t);
void arm_mps2_uart_init(const struct arm_mps2_uart_desc *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* dev_arm_mps2_uart_h */
