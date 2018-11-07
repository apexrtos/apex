#include "mps2-uart.h"

#include "init.h"
#include <arch.h>
#include <dev/tty/tty.h>
#include <irq.h>
#include <kmem.h>

/*
 * Hardware registers
 */
#define CPPREG(name) \
	name() : r{0} { } \
	name(auto v) : r{v} { }
struct mps2_uart {
	uint32_t DATA;
	union state {
		CPPREG(state);
		struct {
			uint32_t TX_FULL : 1;
			uint32_t RX_FULL : 1;
			uint32_t TX_OVERRUN : 1;
			uint32_t RX_OVERRUN : 1;
			uint32_t : 28;
		};
		uint32_t r;
	} STATE;
	union ctrl {
		CPPREG(ctrl);
		struct {
			uint32_t TX_ENABLE : 1;
			uint32_t RX_ENABLE : 1;
			uint32_t TX_INTERRUPT_ENABLE : 1;
			uint32_t RX_INTERRUPT_ENABLE : 1;
			uint32_t TX_OVERRUN_INTERRUPT_ENABLE : 1;
			uint32_t RX_OVERRUN_INTERRUPT_ENABLE : 1;
			uint32_t TX_HIGH_SPEED_TEST_MODE : 1;
		};
		uint32_t r;
	} CTRL;
	union int_status_clear {
		CPPREG(int_status_clear);
		struct {
			uint32_t TX : 1;
			uint32_t RX : 1;
			uint32_t TX_OVERRUN : 1;
			uint32_t RX_OVERRUN : 1;
		};
		uint32_t r;
	} INT_STATUS_CLEAR;
	uint32_t BAUDDIV;
};

/*
 * Early initialisation of UART for kernel debugging
 */
void
arm_mps2_uart_early_init(unsigned long base, tcflag_t cflag)
{
	auto u = reinterpret_cast<mps2_uart *>(base);

	/* TODO: process cflag */
	write32(&u->BAUDDIV, 16);    /* QEMU doesn't care as long as >= 16 */
	write32(&u->CTRL, []{
		union mps2_uart::ctrl c;
		c.TX_ENABLE = 1;
		return c.r;
	}());
}

/*
 * Early printing for kernel debugging
 */
void
arm_mps2_uart_early_print(unsigned long base, const char *s, size_t len)
{
	auto u = reinterpret_cast<mps2_uart *>(base);

	auto putch = [&u](char c) {
		while (read32(&u->STATE).TX_FULL);
		write32(&u->DATA, c);
	};

	while (len) {
		if (*s == '\n')
			putch('\r');
		putch(*s++);
		--len;
	}
}

/*
 * Receive interrupt
 */
static int
rx_isr(int vector, void *data)
{
	auto tp = static_cast<tty *>(data);
	auto u = static_cast<mps2_uart *>(tty_data(tp));

	write32(&u->INT_STATUS_CLEAR, []{
		mps2_uart::int_status_clear s;
		s.RX = 1;
		return s.r;
	}());

	while (read32(&u->STATE).RX_FULL)
		tty_input(tp, read32(&u->DATA));

	return INT_DONE;
}

/*
 * Transmit interrupt
 */
static int
tx_isr(int vector, void *data)
{
	auto tp = static_cast<tty *>(data);
	auto u = static_cast<mps2_uart *>(tty_data(tp));

	write32(&u->INT_STATUS_CLEAR, []{
		mps2_uart::int_status_clear s;
		s.TX = 1;
		return s.r;
	}());

	while (!read32(&u->STATE).TX_FULL) {
		int c;
		if ((c = tty_oq_getc(tp)) < 0) {
			tty_oq_done(tp);
			break;
		}
		write32(&u->DATA,  c);
	}

	return INT_DONE;
}

/*
 * Called whenever UART hardware needs to be reconfigured
 */
static int
tproc(tty *tp)
{
	auto u = static_cast<mps2_uart *>(tty_data(tp));
	auto termios = tty_termios(tp);
	const bool rx = termios->c_cflag & CREAD;

	write32(&u->BAUDDIV, 16);	    /* QEMU doesn't care as long as >= 16 */
	write32(&u->CTRL, [&]{
		mps2_uart::ctrl c;
		c.TX_ENABLE = 1;
		c.RX_ENABLE = rx;
		c.TX_INTERRUPT_ENABLE = 1;
		c.RX_INTERRUPT_ENABLE = rx;
		return c.r;
	}());

	return 0;
}

/*
 * Called whenever UART output should start.
 */
static void
oproc(tty *tp)
{
	const int s = irq_disable();
	tx_isr(0, tp);
	irq_restore(s);
}

/*
 * Initialize
 */
void
arm_mps2_uart_init(const arm_mps2_uart_desc *d)
{
	auto tp = tty_create(d->name, tproc, oproc, reinterpret_cast<void *>(d->base));
	irq_attach(d->rx_int, d->ipl, 0, rx_isr, NULL, tp);
	irq_attach(d->tx_int, d->ipl, 0, tx_isr, NULL, tp);
}
