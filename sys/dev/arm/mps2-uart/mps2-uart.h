#pragma once

#include <cstdint>

/*
 * Hardware registers
 */
namespace arm {

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

}
