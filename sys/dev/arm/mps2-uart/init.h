#pragma once

/*
 * Device driver for UART on ARM MPS2 board
 *
 * Note: This driver has only been tested with QEMU and will not currently work
 * with real hardware.
 *
 */

struct arm_mps2_uart_desc {
	const char *name;
	unsigned long base;
	int ipl;
	int rx_int;
	int tx_int;
	int overflow_int;
};

void arm_mps2_uart_init(const arm_mps2_uart_desc *);
