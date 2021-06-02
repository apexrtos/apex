#include "init.h"

#include "ns16550.h"
#include <debug.h>
#include <dev/tty/tty.h>
#include <irq.h>
#include <sync.h>

namespace {

/*
 * Hardware instance
 */
class ns16550_inst {
public:
	ns16550_inst(const serial_ns16550_desc *d)
	: uart{d->base}
	, clock{d->clock}
	{ }

	serial::ns16550 uart;
	const unsigned long clock;

	a::spinlock_irq lock;
};

/*
 * Get hardware instance from tty
 */
ns16550_inst *
get_inst(tty *tp)
{
	return static_cast<ns16550_inst *>(tty_data(tp));
}

/*
 * Interrupt service routine
 */
int
isr(int vector, void *data)
{
	const auto tp = static_cast<tty *>(data);
	const auto inst = get_inst(tp);
	auto &u = inst->uart;

	while (u.data_ready())
		tty_rx_putc(tp, u.getch());

	while (u.tx_empty()) {
		int c;
		if ((c = tty_tx_getc(tp)) < 0) {
			tty_tx_complete(tp);
			u.txint_enable(false);
			break;
		}
		u.putch(c);
	}

	return INT_DONE;
}

/*
 * Called whenever UART hardware needs to be reconfigured
 */
int
tproc(tty *tp, tcflag_t cflag)
{
	const auto inst = get_inst(tp);
	const bool rx = cflag & CREAD;
	auto &u = inst->uart;

	/* REVISIT: process termios properly */

	std::lock_guard l{inst->lock};
	u.rxint_enable(rx);

	return 0;
}

/*
 * Called whenever UART output should start.
 */
void
oproc(tty *tp)
{
	const auto inst = get_inst(tp);
	auto &u = inst->uart;

	std::lock_guard l{inst->lock};
	u.txint_enable(true);
}

}

/*
 * Initialise
 */
void
serial_ns16550_init(const serial_ns16550_desc *d)
{
	auto tp = tty_create(d->name, MA_NORMAL, 128, 1, tproc, oproc, nullptr,
			     nullptr, new ns16550_inst{d});
	if (tp > (void *)-4096UL)
		panic("tty_create");
	irq_attach(d->irq, d->ipl, d->irq_mode, isr, nullptr, tp);
}
