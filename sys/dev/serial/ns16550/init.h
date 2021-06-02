#pragma once

/*
 * Device driver for National Semiconductor 16550 compatible UART
 */

struct serial_ns16550_desc {
	const char *name;
	unsigned long base;
	unsigned long clock;
	int ipl;
	int irq;
	int irq_mode;
};

void serial_ns16550_init(const serial_ns16550_desc *);
