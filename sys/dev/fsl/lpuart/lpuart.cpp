#include "lpuart.h"

#include "regs.h"
#include <cmath>
#include <debug.h>
#include <dev/tty/tty.h>
#include <device.h>
#include <errno.h>
#include <irq.h>
#include <kernel.h>
#include <kmem.h>
#include <limits.h>

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
		GLOBAL.r = decltype(GLOBAL){
			.RST = 1,
		}.r;
		GLOBAL.r = decltype(GLOBAL){
			.RST = 0,
		}.r;
	}

	void configure(unsigned sbr, unsigned osr, DataBits db,
	    Parity p, StopBits sb, Direction dir, Interrupts ints)
	{
		const auto ien = ints == Interrupts::Enabled;

		/* disable receiver & transmitter during reconfiguration */
		CTRL.r = 0;
		BAUD.r = [&]{
			decltype(BAUD) v{};
			v.SBR = sbr;
			v.SBNS = static_cast<unsigned>(sb);
			v.BOTHEDGE = true;
			v.OSR = osr - 1;
			return v.r;
		}();
		FIFO.r = [&]{
			decltype(FIFO) v{};
			v.RXFE = ien;
			v.TXFE = ien;
			v.RXIDEN = ien ? 1 : 0;
			return v.r;
		}();
		WATER.r = [&]{
			decltype(WATER) v{};
			v.RXWATER = rxfifo_size() - 1;
			v.TXWATER = 0;
			return v.r;
		}();
		CTRL.r = [&]{
			decltype(CTRL) v{};
			if (p != Parity::Disabled) {
				v.PE = true;
				v.PT = static_cast<unsigned>(p) - 1;
			}
			v.M = static_cast<unsigned>(db);
			v.RE = dir == Direction::RxTx;
			v.TE = true;
			v.RIE = ien;
			return v.r;
		}();
	}

	void txint_disable()
	{
		CTRL.TIE = false;
	}

	void txint_enable()
	{
		CTRL.TIE = true;
	}

	void putch_polled(const char c)
	{
		while (!STAT.TDRE);
		putch(c);
	}

	char getch() const
	{
		return DATA;
	}

	void putch(const char c)
	{
		DATA = c;
	}

	std::size_t txcount() const
	{
		return WATER.TXCOUNT;
	}

	std::size_t rxcount() const
	{
		return WATER.RXCOUNT;
	}

	std::size_t txfifo_size() const
	{
		return 1 << PARAM.TXFIFO;
	}

	std::size_t rxfifo_size() const
	{
		return 1 << PARAM.RXFIFO;
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
		const int sbr = div_closest(clock, speed * osr);
		if (sbr == 0)
			continue;
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
	return reinterpret_cast<lpuart_inst*>(tty_data(tp));
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
	const auto tp = reinterpret_cast<tty*>(data);
	const auto inst = get_inst(tp);
	const auto u = inst->uart;

	for (size_t i = u->rxcount(); i > 0; --i)
		tty_input(tp, u->getch());

	for (size_t i = u->txfifo_size() - u->txcount(); i > 0; --i) {
		if (const int c = tty_oq_getc(tp); c >= 0)
			u->putch(c);
		else {
			u->txint_disable();
			tty_oq_done(tp);
			break;
		}
	}

	return INT_DONE;
}

/*
 * Called whenever UART hardware needs to be reconfigured
 */
static int
tproc(tty *tp)
{
	const auto inst = get_inst(tp);
	const auto u = inst->uart;
	const auto tio = tty_termios(tp);
	const bool rx = tio->c_cflag & CREAD;
	const auto speed = tty_speed(tio->c_cflag);

	if (speed < 0)
		return DERR(-EINVAL);

	/* REVISIT: process termios properly */

	auto div = calculate_dividers(inst->clock, speed);
	if (!div.sbr)
		return DERR(-ENOTSUP);

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

	const int s = irq_disable();
	u->txint_enable();
	irq_restore(s);
}

/*
 * Initialise
 */
void
fsl_lpuart_init(const fsl_lpuart_desc *d)
{
	auto tp = tty_create(d->name, tproc, oproc,
	    reinterpret_cast<void*>(new lpuart_inst{d}));
	irq_attach(d->rx_tx_int, d->ipl, 0, isr, NULL, tp);
}
