#ifndef dev_arm_armv7m_systick_init_h
#define dev_arm_armv7m_systick_init_h

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

#endif /* dev_arm_armv7m_systick_h */
