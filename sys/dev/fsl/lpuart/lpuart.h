#pragma once

/*
 * Hardware abstraction for Freescale LPUART
 */

#include <cstddef>
#include <optional>

namespace fsl {

class lpuart {
public:
	lpuart(unsigned long base);

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

	struct dividers {
		unsigned sbr;
		unsigned osr;
	};

	void reset();
	void configure(const dividers &, DataBits db, Parity p, StopBits sb,
		       Direction dir, Interrupts ints);
	void txint_disable();
	void tcint_disable();
	void txints_enable();
	void flush(int io);
	bool tx_complete();
	bool overrun();
	void clear_overrun();
	void putch_polled(const char c);
	char getch() const;
	void putch(const char c);
	std::size_t txcount() const;
	std::size_t rxcount() const;
	std::size_t txfifo_size() const;
	std::size_t rxfifo_size() const;

	static std::optional<dividers>
	calculate_dividers(long clock, long speed);

private:
	struct regs;
	lpuart(lpuart &&) = delete;
	lpuart(const lpuart &) = delete;
	lpuart &operator=(lpuart &&) = delete;
	lpuart &operator=(const lpuart&) = delete;

	regs *const r_;
};

}
