#pragma once

/*
 * Device driver for SysTick timer found in ARMv7-M cores
 */

struct arm_armv7m_systick_desc {
	unsigned long clock;
	int clksource;
};

void arm_armv7m_systick_init(const struct arm_armv7m_systick_desc *);
