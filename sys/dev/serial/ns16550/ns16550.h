#pragma once

/*
 * Device driver for National Semiconductor 16550 compatible UART
 */

#include <cstddef>
#include <termios.h>

namespace serial {

class ns16550 {
public:
	ns16550(unsigned long base);

	void rxint_enable(bool);
	void txint_enable(bool);
	void putch(char);
	void putch_polled(char);
	char getch();
	bool data_ready();
	bool tx_empty();

	/*
	 * Kernel & bootloader early entry points
	 */
	static void early_init(unsigned long base, unsigned long clock, tcflag_t);
	static void early_print(unsigned long base, const char *, size_t);

private:
	struct regs;
	ns16550(ns16550 &&) = delete;
	ns16550(const ns16550 &) = delete;
	ns16550 &operator=(ns16550 &&) = delete;
	ns16550 &operator=(const ns16550&) = delete;

	regs *r_;
};

}
