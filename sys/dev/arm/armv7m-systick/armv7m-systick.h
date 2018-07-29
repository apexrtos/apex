#ifndef dev_arm_armv7m_systick_h
#define dev_arm_armv7m_systick_h

/*
 * Device driver for SysTick timer found in ARMv7-M cores
 */

#ifdef __cplusplus
extern "C" {
#endif

struct armv7m_systick_desc {
	unsigned long clock;
	int clksource;
};

void armv7m_systick_init(const struct armv7m_systick_desc *);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* dev_arm_armv7m_systick_h */
