/*
 * crt_init.S
 *
 * initialise C runtime environment and call loader_main
 */

.text
.global crt_init
crt_init:
	/* copy data */
	la a0, __data_rw
	la a1, __data_ro
	la a2, __data_size
	call memcpy

	/* zero bss */
	la a0, __bss_start
	li a1, 0
	la a2, __bss_size
	call memset

	/* jump to loader_main */
	tail loader_main
