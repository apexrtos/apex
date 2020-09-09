#include "lpuart.h"

#include "init.h"
#include "regs.h"
#include <algorithm>
#include <arch.h>
#include <cstdlib>
#include <debug.h>
#include <dev/tty/tty.h>
#include <errno.h>
#include <irq.h>
#include <kernel.h>
#include <limits.h>
#include <sync.h>

namespace {

/*
 * Hardware abstraction
 */
enum class DataBits {
	Eight,
	Nine,
};

enum class StopBits {
	One,
	Two,
};

enum class Parity {
	Disabled,
	Even,
	Odd,
};

enum class Direction {
	Tx,
	RxTx,
};

enum class Interrupts {
	Disabled,
	Enabled,
};

class lpuart : private lpuart_regs {
public:
	void reset()
	{
		write32(&GLOBAL, [&]{
			decltype(GLOBAL) g{};
			g.RST = true;
			return g.r;
		}());
		write32(&GLOBAL, 0);
	}

	void configure(unsigned sbr, unsigned osr, DataBits db,
	    Parity p, StopBits sb, Direction dir, Interrupts ints)
	{
		const auto ien = ints == Interrupts::Enabled;

		/* disable receiver & transmitter during reconfiguration */
		write32(&CTRL, 0);
		write32(&BAUD, [&]{
			decltype(BAUD) v{};
			v.SBR = sbr;
			v.SBNS = static_cast<unsigned>(sb);
			v.BOTHEDGE = true;
			v.OSR = osr - 1;
			return v.r;
		}());
		write32(&FIFO, [&]{
			decltype(FIFO) v{};
			v.RXFE = ien;
			v.TXFE = ien;
			v.RXIDEN = ien ? 1 : 0;
			return v.r;
		}());
		write32(&WATER, [&]{
			decltype(WATER) v{};
			v.RXWATER = 1;
			v.TXWATER = 0;
			return v.r;
		}());
		write32(&CTRL, [&]{
			decltype(CTRL) v{};
			if (p != Parity::Disabled) {
				v.PE = true;
				v.PT = static_cast<unsigned>(p) - 1;
			}
			v.M = static_cast<unsigned>(db);
			v.RE = dir == Direction::RxTx;
			v.TE = true;
			v.RIE = ien;
			v.ORIE = ien;
			return v.r;
		}());
	}

	void txint_disable()
	{
		auto v = read32(&CTRL);
		v.TIE = false;
		write32(&CTRL, v.r);
	}

	void tcint_disable()
	{
		auto v = read32(&CTRL);
		v.TCIE = false;
		write32(&CTRL, v.r);
	}

	void txints_enable()
	{
		auto v = read32(&CTRL);
		v.TIE = true;
		v.TCIE = true;
		write32(&CTRL, v.r);
	}

	void flush(int io)
	{
		auto v = read32(&FIFO);
		v.RXFLUSH = io == TCIFLUSH || io == TCIOFLUSH;
		v.TXFLUSH = io == TCOFLUSH || io == TCIOFLUSH;
		write32(&FIFO, v.r);
	}

	bool tx_complete()
	{
		return read32(&STAT).TC;
	}

	bool overrun()
	{
		return read32(&STAT).OR;
	}

	void clear_overrun()
	{
		write32(&STAT, []{
			decltype(STAT) v{};
			v.OR = 1;
			return v.r;
		}());
	}

	void putch_polled(const char c)
	{
		while (!read32(&STAT).TDRE);
		putch(c);
	}

	char getch() const
	{
		return read32(&DATA);
	}

	void putch(const char c)
	{
		write32(&DATA, c);
	}

	std::size_t txcount() const
	{
		return read32(&WATER).TXCOUNT;
	}

	std::size_t rxcount() const
	{
		return read32(&WATER).RXCOUNT;
	}

	std::size_t txfifo_size() const
	{
		return 1 << read32(&PARAM).TXFIFO;
	}

	std::size_t rxfifo_size() const
	{
		return 1 << read32(&PARAM).RXFIFO;
	}
};
static_assert(sizeof(lpuart) == sizeof(lpuart_regs), "");

/*
 * Hardware instance
 */
class lpuart_inst {
public:
	lpuart_inst(const fsl_lpuart_desc *d)
	: uart{reinterpret_cast<lpuart*>(d->base)}
	, clock{d->clock}
	{ }

	lpuart *const uart;
	const unsigned long clock;

	a::spinlock_irq lock;
};

/*
 * Calculate best dividers to get the baud rate we want.
 *
 * Prefer higher oversampling ratios
 *
 * baud = clock / (SBR * OSR)
 */
auto
calculate_dividers(const long clock, const long speed)
{
	struct {
		unsigned sbr;
		unsigned osr;
	} r = {0, 0};
	long error = LONG_MAX;

	for (int osr = 4; osr <= 32; ++osr) {
		const int sbr = std::min(div_closest(clock, speed * osr), 8191l);
		if (sbr == 0)
			break;
		const auto e = std::abs(speed - clock / (osr * sbr));
		if (e <= error) {
			error = e;
			r.sbr = sbr;
			r.osr = osr;
		}
	}

	/* fail if baud rate error is >= 3% */
	if (error * 100 / speed > 3)
		r.sbr = 0;

	return r;
}

/*
 * Get hardware instance from tty
 */
lpuart_inst *
get_inst(tty *tp)
{
	return static_cast<lpuart_inst*>(tty_data(tp));
}

}

/*
 * Early initialisation of UART for kernel debugging
 */
void
fsl_lpuart_early_init(unsigned long base, unsigned long clock, tcflag_t cflag)
{
	lpuart *const u = reinterpret_cast<lpuart*>(base);

	/* TODO: process more of cflag? */

	/*
	 * Configure baud rate
	 */
	const int speed = tty_speed(cflag);
	if (speed < 0)
		panic("invalid speed");

	auto div = calculate_dividers(clock, speed);
	if (!div.sbr)
		panic("invalid speed");

	u->configure(div.sbr, div.osr, DataBits::Eight, Parity::Disabled,
	    StopBits::One, Direction::Tx, Interrupts::Disabled);
}

/*
 * Early printing for kernel debugging
 */
void
fsl_lpuart_early_print(unsigned long base, const char *s, size_t len)
{
	lpuart *const u = reinterpret_cast<lpuart*>(base);

	while (len) {
		if (*s == '\n')
			u->putch_polled('\r');
		u->putch_polled(*s++);
		--len;
	}
}

/*
 * Transmit & receive interrupt service routine
 */
static int
isr(int vector, void *data)
{
	const auto tp = static_cast<tty*>(data);
	const auto inst = get_inst(tp);
	const auto u = inst->uart;

	std::lock_guard l{inst->lock};

	for (size_t i = u->rxcount(); i > 0; --i)
		tty_rx_putc(tp, u->getch());

	if (u->overrun()) {
		tty_rx_overflow(tp);
		u->clear_overrun();
	}

	bool tx_queued = false;
	for (size_t i = u->txfifo_size() - u->txcount(); i > 0; --i) {
		const int c = tty_tx_getc(tp);
		if (c < 0)
			break;
		u->putch(c);
		tx_queued = true;
	}
	if (!tx_queued)
		u->txint_disable();

	if (u->tx_complete()) {
		u->tcint_disable();
		tty_tx_complete(tp);
	}

	return INT_DONE;
}

/*
 * Called whenever UART hardware needs to be reconfigured
 */
static int
tproc(tty *tp, tcflag_t cflag)
{
	const auto inst = get_inst(tp);
	const auto u = inst->uart;
	const bool rx = cflag & CREAD;
	const auto speed = tty_speed(cflag);

	if (speed < 0)
		return DERR(-EINVAL);

	/* REVISIT: process termios properly */

	auto div = calculate_dividers(inst->clock, speed);
	if (!div.sbr)
		return DERR(-ENOTSUP);

	std::lock_guard l{inst->lock};
	u->configure(div.sbr, div.osr,
	    DataBits::Eight,
	    Parity::Disabled,
	    StopBits::One,
	    rx ? Direction::RxTx : Direction::Tx,
	    Interrupts::Enabled);

	return 0;
}

/*
 * Called whenever UART output should start.
 */
static void
oproc(tty *tp)
{
	const auto inst = get_inst(tp);
	const auto u = inst->uart;

	std::lock_guard l{inst->lock};
	u->txints_enable();
}

/*
 * Called to flush UART input, output or both
 */
static void
fproc(tty *tp, int io)
{
	const auto inst = get_inst(tp);
	const auto u = inst->uart;

	std::lock_guard l{inst->lock};
	u->flush(io);
}

/*
 * Initialise
 */
void
fsl_lpuart_init(const fsl_lpuart_desc *d)
{
	auto tp = tty_create(d->name, MA_NORMAL, 128, 1, tproc, oproc,
	    nullptr, fproc, new lpuart_inst{d});
	if (tp > (void *)-4096UL)
		panic("tty_create");
	irq_attach(d->rx_tx_int, d->ipl, 0, isr, NULL, tp);
}
