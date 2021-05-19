#include "early.h"

#include "mps2-uart.h"
#include <sys/include/arch/mmio.h>

namespace arm {

/*
 * Early initialisation of UART for kernel & bootloader debugging
 */
void
mps2_uart_early_init(unsigned long base, tcflag_t cflag)
{
	auto u = reinterpret_cast<mps2_uart *>(base);

	/* TODO: process cflag */
	write32(&u->BAUDDIV, 16u); /* QEMU doesn't care as long as >= 16 */
	write32(&u->CTRL, {.TX_ENABLE = 1});
}

/*
 * Early printing for kernel & bootloader debugging
 */
void
mps2_uart_early_print(unsigned long base, const char *s, size_t len)
{
	auto u = reinterpret_cast<mps2_uart *>(base);

	auto putch = [&u](char c) {
		while (read32(&u->STATE).TX_FULL);
		write32(&u->DATA, static_cast<uint32_t>(c));
	};

	while (len) {
		if (*s == '\n')
			putch('\r');
		putch(*s++);
		--len;
	}
}

}
