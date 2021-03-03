#pragma once

/*
 * Device driver for SysTick timer found in ARMv7-M cores
 */

#ifdef __cplusplus
extern "C" {
#endif

struct arm_armv7m_systick_desc {
	unsigned long clock;
	int clksource;
};

void arm_armv7m_systick_init(const struct arm_armv7m_systick_desc *);

#ifdef __cplusplus
} /* extern "C" */
#endif
