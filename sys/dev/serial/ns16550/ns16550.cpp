#include "ns16550.h"

#include <cstdint>
#include <lib/bitfield.h>
#include <sys/include/arch/mmio.h>

namespace bf = bitfield;

namespace serial {

/*
 * ns16550 hardware registers
 */
struct ns16550::regs {
	uint8_t RHR_THR;
	union {
		using S = uint8_t;
		struct { S r; };
		bf::bit<S, bool, 7> ERBFI;  /* Received Data Available */
		bf::bit<S, bool, 6> ETBEI;  /* Transmit Hold Register Empty */
		bf::bit<S, bool, 5> ELSI;   /* Receive Line Status */
		bf::bit<S, bool, 4> EDSSI;  /* Modem Status */
	} IER;
	uint8_t IIR_FCR;
	uint8_t LCR;
	uint8_t MCR;
	union {
		using S = uint8_t;
		struct { S r; };
		bf::bit<S, bool, 7> FDE;    /* FIFO Data Error */
		bf::bit<S, bool, 6> TEMT;   /* Transmitter Empty */
		bf::bit<S, bool, 5> THRE;   /* Transmit Hold Register Empty */
		bf::bit<S, bool, 4> BI;	    /* Break Interrupt */
		bf::bit<S, bool, 3> FE;	    /* Framing Error */
		bf::bit<S, bool, 2> PE;	    /* Parity Error */
		bf::bit<S, bool, 1> OE;	    /* Overrun Error */
		bf::bit<S, bool, 0> DR;	    /* Data Ready */
	} LSR;
	uint8_t MSR;
	uint8_t SCR;
};

ns16550::ns16550(unsigned long base)
#warning translate base address!! phys_to_virt?? vm_translate?? -- base is > 32 bits!!
: r_{reinterpret_cast<regs *>(base)}
{
	static_assert(sizeof(ns16550::regs) == 8);
}

void
ns16550::rxint_enable(bool en)
{
	auto r = read8(&r_->IER);
	r.ERBFI = en;
	write8(&r_->IER, r);
}

void
ns16550::txint_enable(bool en)
{
	auto r = read8(&r_->IER);
	r.ETBEI = en;
	write8(&r_->IER, r);
}

void
ns16550::putch(char c)
{
	write8(&r_->RHR_THR, static_cast<uint8_t>(c));
}

void
ns16550::putch_polled(char c)
{
	while (!tx_empty());
	putch(c);
}

char
ns16550::getch()
{
	return read8(&r_->RHR_THR);
}

bool
ns16550::data_ready()
{
	return read8(&r_->LSR).DR;
}

bool
ns16550::tx_empty()
{
	return read8(&r_->LSR).THRE;
}

/*
 * Early initialisation of UART for kernel & bootloader debugging
 */
void
ns16550::early_init(unsigned long base, unsigned long clock, tcflag_t)
{
	/* TODO: QEMU doesn't need initialisation */
}

/*
 * Early printing for kernel & bootloader debugging
 */
void
ns16550::early_print(unsigned long base, const char *s, size_t len)
{
	ns16550 u{base};

	while (len--) {
		if (*s == '\n')
			u.putch_polled('\r');
		u.putch_polled(*s++);
	}

}

}
