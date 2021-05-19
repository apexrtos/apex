#include "init.h"

#include "mps2-uart.h"
#include <arch/mmio.h>
#include <debug.h>
#include <dev/tty/tty.h>
#include <irq.h>
#include <types.h>

namespace {

/*
 * Receive interrupt
 */
int
rx_isr(int vector, void *data)
{
	auto tp = static_cast<tty *>(data);
	auto u = static_cast<arm::mps2_uart *>(tty_data(tp));

	write32(&u->INT_STATUS_CLEAR, {.RX = 1});

	while (read32(&u->STATE).RX_FULL)
		tty_rx_putc(tp, read32(&u->DATA));

	return INT_DONE;
}

/*
 * Transmit interrupt
 */
int
tx_isr(int vector, void *data)
{
	auto tp = static_cast<tty *>(data);
	auto u = static_cast<arm::mps2_uart *>(tty_data(tp));

	write32(&u->INT_STATUS_CLEAR, {.TX = 1});

	while (!read32(&u->STATE).TX_FULL) {
		int c;
		if ((c = tty_tx_getc(tp)) < 0) {
			tty_tx_complete(tp);
			break;
		}
		write32(&u->DATA, static_cast<uint32_t>(c));
	}

	return INT_DONE;
}

/*
 * Called whenever UART hardware needs to be reconfigured
 */
int
tproc(tty *tp, tcflag_t cflag)
{
	auto u = static_cast<arm::mps2_uart *>(tty_data(tp));
	const bool rx = cflag & CREAD;

	write32(&u->BAUDDIV, 16u);  /* QEMU doesn't care as long as >= 16 */
	write32(&u->CTRL, [&]{
		arm::mps2_uart::ctrl r{};
		r.TX_ENABLE = 1;
		r.RX_ENABLE = rx;
		r.TX_INTERRUPT_ENABLE = 1;
		r.RX_INTERRUPT_ENABLE = rx;
		return r;
	}());

	return 0;
}

/*
 * Called whenever UART output should start.
 */
void
oproc(tty *tp)
{
	const int s = irq_disable();
	tx_isr(0, tp);
	irq_restore(s);
}

}

/*
 * Initialize
 */
void
arm_mps2_uart_init(const arm_mps2_uart_desc *d)
{
	auto tp = tty_create(d->name, MA_NORMAL, 128, 1, tproc, oproc,
	    nullptr, nullptr, reinterpret_cast<void *>(d->base));
	if (tp > (void *)-4096UL)
		panic("tty_create");
	irq_attach(d->rx_int, d->ipl, 0, rx_isr, nullptr, tp);
	irq_attach(d->tx_int, d->ipl, 0, tx_isr, nullptr, tp);
}
