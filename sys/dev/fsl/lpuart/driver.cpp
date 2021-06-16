#include "lpuart.h"

#include "init.h"
#include <debug.h>
#include <dev/tty/helpers.h>
#include <dev/tty/tty.h>
#include <irq.h>
#include <sync.h>

namespace {

/*
 * Hardware instance
 */
class lpuart_inst {
public:
	lpuart_inst(const fsl_lpuart_desc *d)
	: uart{d->base}
	, clock{d->clock}
	{ }

	fsl::lpuart uart;
	const unsigned long clock;

	a::spinlock_irq lock;
};

/*
 * Get hardware instance from tty
 */
lpuart_inst *
get_inst(tty *tp)
{
	return static_cast<lpuart_inst*>(tty_data(tp));
}

/*
 * Transmit & receive interrupt service routine
 */
int
isr(int vector, void *data)
{
	const auto tp = static_cast<tty*>(data);
	const auto inst = get_inst(tp);
	auto &u = inst->uart;

	std::lock_guard l{inst->lock};

	for (size_t i = u.rxcount(); i > 0; --i)
		tty_rx_putc(tp, u.getch());

	if (u.overrun()) {
		tty_rx_overflow(tp);
		u.clear_overrun();
	}

	bool tx_queued = false;
	for (size_t i = u.txfifo_size() - u.txcount(); i > 0; --i) {
		const int c = tty_tx_getc(tp);
		if (c < 0)
			break;
		u.putch(c);
		tx_queued = true;
	}
	if (!tx_queued)
		u.txint_disable();

	if (u.tx_complete()) {
		u.tcint_disable();
		tty_tx_complete(tp);
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
	const auto speed = tty_speed(cflag);
	auto &u = inst->uart;

	if (speed < 0)
		return DERR(-EINVAL);

	/* REVISIT: process termios properly */

	auto div = fsl::lpuart::calculate_dividers(inst->clock, speed);
	if (!div)
		return DERR(-ENOTSUP);

	std::lock_guard l{inst->lock};
	u.configure(*div,
	    fsl::lpuart::DataBits::Eight,
	    fsl::lpuart::Parity::Disabled,
	    fsl::lpuart::StopBits::One,
	    rx ? fsl::lpuart::Direction::RxTx : fsl::lpuart::Direction::Tx,
	    fsl::lpuart::Interrupts::Enabled);

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
	u.txints_enable();
}

/*
 * Called to flush UART input, output or both
 */
void
fproc(tty *tp, int io)
{
	const auto inst = get_inst(tp);
	auto &u = inst->uart;

	std::lock_guard l{inst->lock};
	u.flush(io);
}

}

/*
 * Initialise
 */
void
fsl_lpuart_init(const fsl_lpuart_desc *d)
{
	auto tp = tty_create(d->name, MA_NORMAL, 128, 1, tproc, oproc,
	    nullptr, fproc, new lpuart_inst{d});
	if (!tp.ok())
		panic("tty_create");
	irq_attach(d->rx_tx_int, d->ipl, 0, isr, nullptr, tp.val());
}
