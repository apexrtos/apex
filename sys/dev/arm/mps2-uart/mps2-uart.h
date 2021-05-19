#pragma once

#include <cstdint>
#include <sys/lib/bitfield.h>

/*
 * Hardware registers
 *
 *
 */
namespace arm {

struct mps2_uart {
	uint32_t DATA;
	union state {
		using S = uint32_t;
		struct { S r; };
		bitfield::bit<S, bool, 3> RX_OVERRUN;
		bitfield::bit<S, bool, 2> TX_OVERRUN;
		bitfield::bit<S, bool, 1> RX_FULL;
		bitfield::bit<S, bool, 0> TX_FULL;
	} STATE;
	union ctrl {
		using S = uint32_t;
		struct { S r; };
		bitfield::bit<S, bool, 6> TX_HIGH_SPEED_TEST_MODE;
		bitfield::bit<S, bool, 5> RX_OVERRUN_INTERRUPT_ENABLE;
		bitfield::bit<S, bool, 4> TX_OVERRUN_INTERRUPT_ENABLE;
		bitfield::bit<S, bool, 3> RX_INTERRUPT_ENABLE;
		bitfield::bit<S, bool, 2> TX_INTERRUPT_ENABLE;
		bitfield::bit<S, bool, 1> RX_ENABLE;
		bitfield::bit<S, bool, 0> TX_ENABLE;
	} CTRL;
	union int_status_clear {
		using S = uint32_t;
		struct { S r; };
		bitfield::bit<S, bool, 3> RX_OVERRUN;
		bitfield::bit<S, bool, 2> TX_OVERRUN;
		bitfield::bit<S, bool, 1> RX;
		bitfield::bit<S, bool, 0> TX;
	} INT_STATUS_CLEAR;
	uint32_t BAUDDIV;
};

}
