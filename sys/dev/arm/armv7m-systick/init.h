#pragma once

/*
 * Device driver for SysTick timer found in ARMv7-M cores
 */

struct arm_armv7m_systick_desc {
	unsigned long clock;
	bool clksource;
};

void arm_armv7m_systick_init(const arm_armv7m_systick_desc *);
