/*
 * head.S
 *
 * entry point for boot loader
 */

.text
.global _start
_start:
	/* CPU starts in M mode with interrupts disabled */

	/* set global pointer */
.option push
.option norelax
	la gp, __global_pointer
.option pop

	/* set stack pointer */
	la sp, __stack_top

	/* jump to crt_init */
	tail crt_init
