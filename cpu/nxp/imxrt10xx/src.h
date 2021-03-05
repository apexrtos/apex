#pragma once

#include <cassert>
#include <cstdint>

/*
 * System Reset Controller
 */
struct src {
	union src_scr {
		struct {
			uint32_t : 7;
			uint32_t MASK_WDOG_RST : 4;
			uint32_t : 2;
			uint32_t CORE0_RST : 1;
			uint32_t : 3;
			uint32_t CORE0_DBG_RST : 1;
			uint32_t : 7;
			uint32_t DBG_RST_MASK_PG : 1;
			uint32_t : 2;
			uint32_t MASK_WDOG3_RST : 4;
		};
		uint32_t r;
	} SCR;
	uint32_t SBMR1;
	uint32_t SRSR;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t SBMR2;
	uint32_t GPR1;
	uint32_t GPR2;
	uint32_t GPR3;
	uint32_t GPR4;
	uint32_t GPR5;
	uint32_t GPR6;
	uint32_t GPR7;
	uint32_t GPR8;
	uint32_t GPR9;
	uint32_t GPR10;
};
static_assert(sizeof(src) == 0x48, "");

static src *const SRC = (src*)0x400f8000;
