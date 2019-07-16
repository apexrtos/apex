#ifndef arm_v7m_interrupt_h
#define arm_v7m_interrupt_h

#include <conf/config.h>

#define NVIC_STEP (1 << (8 - CONFIG_NVIC_PRIO_BITS))

/*
 * Interrupt priority levels
 */
#define IPL_SVCALL	(256 - 1 * NVIC_STEP)	/* supervisor call */
#define IPL_PENDSV	(256 - 1 * NVIC_STEP)	/* deferred supervisor call */
#define IPL_MIN		(256 - 2 * NVIC_STEP)	/* minimum interrupt priority */
#define IPL_MAX		0			/* maximum interrupt priority */

#endif /* arm_v7m_interrupt_h */
