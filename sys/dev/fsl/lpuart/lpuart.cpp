#include "lpuart.h"

#include <algorithm>
#include <climits>
#include <sys/include/arch/mmio.h>
#include <sys/include/debug.h>
#include <sys/include/kernel.h>
#include <termios.h>

namespace fsl {

/*
 * lpuart::regs - lpuart hardware registers
 */
struct lpuart::regs {
	uint32_t VERID;
	union param {
		struct {
			uint32_t TXFIFO : 8;
			uint32_t RXFIFO : 8;
			uint32_t : 16;
		};
		uint32_t r;
	} PARAM;
	union global {
		struct {
			uint32_t : 1;
			uint32_t RST : 1;
			uint32_t : 30;
		};
		uint32_t r;
	} GLOBAL;
	uint32_t PINCFG;
	union baud {
		struct {
			uint32_t SBR : 13;
			uint32_t SBNS : 1;
			uint32_t RXEDGIE : 1;
			uint32_t LBKDIE : 1;
			uint32_t RESYNCDIS : 1;
			uint32_t BOTHEDGE : 1;
			uint32_t MATCFG : 2;
			uint32_t : 1;
			uint32_t RDMAE : 1;
			uint32_t : 1;
			uint32_t TDMAE : 1;
			uint32_t OSR : 5;
			uint32_t M10 : 1;
			uint32_t MAEN2 : 1;
			uint32_t MAEN1 : 1;
		};
		uint32_t r;
	} BAUD;
	union stat {
		struct {
			uint32_t : 14;
			uint32_t MA2F : 1;
			uint32_t MA1F : 1;
			uint32_t PF : 1;
			uint32_t FE : 1;
			uint32_t NF : 1;
			uint32_t OR : 1;
			uint32_t IDLE : 1;
			uint32_t RDRF : 1;
			uint32_t TC : 1;
			uint32_t TDRE : 1;
			uint32_t RAF : 1;
			uint32_t LBKDE : 1;
			uint32_t BRK13 : 1;
			uint32_t RWUID : 1;
			uint32_t RXINV : 1;
			uint32_t MSBF : 1;
			uint32_t RXEDGIF : 1;
			uint32_t LBKDIF : 1;
		};
		uint32_t r;
	} STAT;
	union ctrl {
		struct {
			uint32_t PT : 1;
			uint32_t PE : 1;
			uint32_t ILT : 1;
			uint32_t WAKE : 1;
			uint32_t M : 1;
			uint32_t RSRC : 1;
			uint32_t DOZEEN : 1;
			uint32_t LOOPS : 1;
			uint32_t IDLECFG : 3;
			uint32_t M7 : 1;
			uint32_t : 2;
			uint32_t MA2IE : 1;
			uint32_t MA1IE : 1;
			uint32_t SBK : 1;
			uint32_t RWU : 1;
			uint32_t RE : 1;
			uint32_t TE : 1;
			uint32_t ILIE : 1;
			uint32_t RIE : 1;
			uint32_t TCIE : 1;
			uint32_t TIE : 1;
			uint32_t PEIE : 1;
			uint32_t FEIE : 1;
			uint32_t NEIE : 1;
			uint32_t ORIE : 1;
			uint32_t TXINV : 1;
			uint32_t TXDIR : 1;
			uint32_t R9T8 : 1;
			uint32_t R8T9 : 1;
		};
		uint32_t r;
	} CTRL;
	uint32_t DATA;
	uint32_t MATCH;
	uint32_t MODIR;
	union fifo {
		struct {
			uint32_t RXFIFOSIZE : 3;
			uint32_t RXFE : 1;
			uint32_t TXFIFOSIZE : 3;
			uint32_t TXFE : 1;
			uint32_t RXUFE : 1;
			uint32_t TXOFE : 1;
			uint32_t RXIDEN : 3;
			uint32_t : 1;
			uint32_t RXFLUSH : 1;
			uint32_t TXFLUSH : 1;
			uint32_t RXUF : 1;
			uint32_t TXOF : 1;
			uint32_t : 4;
			uint32_t RXEMPT : 1;
			uint32_t TXEMPT : 1;
			uint32_t : 8;
		};
		uint32_t r;
	} FIFO;
	union water {
		struct {
			uint32_t TXWATER : 8;
			uint32_t TXCOUNT : 8;
			uint32_t RXWATER : 8;
			uint32_t RXCOUNT : 8;
		};
		uint32_t r;
	} WATER;
};

/*
 * lpuart
 */
lpuart::lpuart(unsigned long base)
: r_{reinterpret_cast<regs *>(base)}
{
	static_assert(sizeof(lpuart::regs) == 0x30);
	static_assert(BYTE_ORDER == LITTLE_ENDIAN);
}

void
lpuart::reset()
{
	write32(&r_->GLOBAL, (regs::global){{
		.RST = true
	}}.r);
	write32(&r_->GLOBAL, 0);
}

void
lpuart::configure(const dividers &div, DataBits db, Parity p, StopBits sb,
		  Direction dir, Interrupts ints)
{
	const auto ien = ints == Interrupts::Enabled;

	/* disable receiver & transmitter during reconfiguration */
	write32(&r_->CTRL, 0);
	write32(&r_->BAUD, (regs::baud){{
		.SBR = div.sbr,
		.SBNS = static_cast<unsigned>(sb),
		.BOTHEDGE = true,
		.OSR = div.osr - 1,
	}}.r);
	write32(&r_->FIFO, (regs::fifo){{
		.RXFE = ien,
		.TXFE = ien,
		.RXIDEN = ien,
	}}.r);
	write32(&r_->WATER, (regs::water){{
		.TXWATER = 0,
		.RXWATER = 1,
	}}.r);
	write32(&r_->CTRL, [&]{
		decltype(r_->CTRL) v{};
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

void
lpuart::txint_disable()
{
	auto v = read32(&r_->CTRL);
	v.TIE = false;
	write32(&r_->CTRL, v.r);
}

void
lpuart::tcint_disable()
{
	auto v = read32(&r_->CTRL);
	v.TCIE = false;
	write32(&r_->CTRL, v.r);
}

void
lpuart::txints_enable()
{
	auto v = read32(&r_->CTRL);
	v.TIE = true;
	v.TCIE = true;
	write32(&r_->CTRL, v.r);
}

void
lpuart::flush(int io)
{
	auto v = read32(&r_->FIFO);
	v.RXFLUSH = io == TCIFLUSH || io == TCIOFLUSH;
	v.TXFLUSH = io == TCOFLUSH || io == TCIOFLUSH;
	write32(&r_->FIFO, v.r);
}

bool
lpuart::tx_complete()
{
	return read32(&r_->STAT).TC;
}

bool
lpuart::overrun()
{
	return read32(&r_->STAT).OR;
}

void
lpuart::clear_overrun()
{
	write32(&r_->STAT, (regs::stat){{
		.OR = 1
	}}.r);
}

void
lpuart::putch_polled(const char c)
{
	while (!read32(&r_->STAT).TDRE);
	putch(c);
}

char
lpuart::getch() const
{
	return read32(&r_->DATA);
}

void
lpuart::putch(const char c)
{
	write32(&r_->DATA, static_cast<uint32_t>(c));
}

std::size_t
lpuart::txcount() const
{
	return read32(&r_->WATER).TXCOUNT;
}

std::size_t
lpuart::rxcount() const
{
	return read32(&r_->WATER).RXCOUNT;
}

std::size_t
lpuart::txfifo_size() const
{
	return 1 << read32(&r_->PARAM).TXFIFO;
}

std::size_t
lpuart::rxfifo_size() const
{
	return 1 << read32(&r_->PARAM).RXFIFO;
}

/*
 * Calculate best dividers to get the baud rate we want.
 *
 * Prefer higher oversampling ratios
 *
 * baud = clock / (SBR * OSR)
 */
std::optional<lpuart::dividers>
lpuart::calculate_dividers(long clock, long speed)
{
	dividers r;
	long error = LONG_MAX;

	for (int osr = 4; osr <= 32; ++osr) {
		auto sbr = std::min(div_closest(clock, speed * osr), 8191l);
		if (sbr == 0)
			break;
		auto e = std::abs(speed - clock / (osr * sbr));
		if (e <= error) {
			error = e;
			r.sbr = sbr;
			r.osr = osr;
		}
	}

	/* fail if baud rate error is >= 3% */
	if (error * 100 / speed > 3)
		return std::nullopt;

	return r;
}

}
