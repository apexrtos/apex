#include "mps2-uart.h"

#include <dev/tty/tty.h>
#include <irq.h>
#include <kmem.h>

/*
 * Hardware registers
 */
struct mps2_uart {
	uint32_t DATA;
	union state {
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
extern "C" void
mps2_uart_early_init(unsigned long base, tcflag_t oflag)
{
	volatile struct mps2_uart *const u = (struct mps2_uart*)base;

	/* TODO: process oflag */
	u->BAUDDIV = 16;	    /* QEMU doesn't care as long as >= 16 */
	u->CTRL.TX_ENABLE = 1;
}

/*
 * Early printing for kernel debugging
 */
extern "C" void
mps2_uart_early_print(unsigned long base, const char *s, size_t len)
{
	volatile struct mps2_uart *const u = (struct mps2_uart*)base;

	auto putch = [&u](char c) {
		while (u->STATE.TX_FULL);
		u->DATA = c;
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
	auto tp = (struct tty *)data;
	auto u = (volatile struct mps2_uart *)tty_data(tp);

	u->INT_STATUS_CLEAR.r = (mps2_uart::int_status_clear){{
		.TX = 0,
		.RX = 1
	}}.r;

	while (u->STATE.RX_FULL)
		tty_input(tp, u->DATA);

	return INT_DONE;
}

/*
 * Transmit interrupt
 */
static int
tx_isr(int vector, void *data)
{
	auto tp = (struct tty *)data;
	auto u = (volatile struct mps2_uart *)tty_data(tp);

	u->INT_STATUS_CLEAR.r = (mps2_uart::int_status_clear){{
		.TX = 1
	}}.r;

	while (!u->STATE.TX_FULL) {
		int c;
		if ((c = tty_oq_getc(tp)) < 0) {
			tty_oq_done(tp);
			break;
		}
		u->DATA = c;
	}

	return INT_DONE;
}

/*
 * Called whenever UART hardware needs to be reconfigured
 */
static int
tproc(struct tty *tp)
{
	auto u = (volatile struct mps2_uart *)tty_data(tp);
	auto termios = tty_termios(tp);
	const bool rx = termios->c_cflag & CREAD;

	u->BAUDDIV = 16;	    /* QEMU doesn't care as long as >= 16 */
	u->CTRL.r = (mps2_uart::ctrl){{
		.TX_ENABLE = 1,
		.RX_ENABLE = rx,
		.TX_INTERRUPT_ENABLE = 1,
		.RX_INTERRUPT_ENABLE = rx,
	}}.r;

	return 0;
}

/*
 * Called whenever UART output should start.
 */
static void
oproc(struct tty *tp)
{
	const int s = irq_disable();
	tx_isr(0, tp);
	irq_restore(s);
}

/*
 * Initialize
 */
void
mps2_uart_init(const mps2_uart_desc *d)
{
	auto tp = tty_create(d->name, tproc, oproc, (void*)d->base);
	irq_attach(d->rx_int, d->ipl, 0, rx_isr, NULL, tp);
	irq_attach(d->tx_int, d->ipl, 0, tx_isr, NULL, tp);
}
