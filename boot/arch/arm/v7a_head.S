/*
 * head.S
 *
 * set stack pointer and jump to crt_init
 */

#include <conf/config.h>

.syntax unified

/*
 * start
 */
.text
.globl start
start:
	/* Cortex-A15 starts in ARM, SVC mode with IRQ and FIQ disabled */

	/* set stack pointer */
	ldr sp, =(CONFIG_BOOTSTACK_BASE_PHYS + CONFIG_BOOTSTACK_SIZE)

	/* jump to crt_init */
	b crt_init
.ltorg
