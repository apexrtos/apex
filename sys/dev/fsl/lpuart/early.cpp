#include "early.h"

#include "lpuart.h"
#include <sys/dev/tty/helpers.h>
#include <sys/include/debug.h>

namespace fsl {

/*
 * Early initialisation of UART for kernel & bootloader debugging
 */
void
lpuart_early_init(unsigned long base, unsigned long clock, tcflag_t cflag)
{
	lpuart u{base};

	/* TODO: process more of cflag? */

	/*
	 * Configure baud rate
	 */
	const int speed = tty_speed(cflag);
	if (speed < 0)
		panic("invalid speed");

	const auto div = lpuart::calculate_dividers(clock, speed);
	if (!div)
		panic("invalid speed");

	u.configure(*div, lpuart::DataBits::Eight, lpuart::Parity::Disabled,
		    lpuart::StopBits::One, lpuart::Direction::Tx,
		    lpuart::Interrupts::Disabled);
}

/*
 * Early printing for kernel & bootloader debugging
 */
void
lpuart_early_print(unsigned long base, const char *s, size_t len)
{
	lpuart u{base};

	while (len) {
		if (*s == '\n')
			u.putch_polled('\r');
		u.putch_polled(*s++);
		--len;
	}
}

}
