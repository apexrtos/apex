/*
 * crt_init.S
 *
 * C runtime initialisation.
 */

.text

/*
 * crt_init
 */
.global crt_init
crt_init:
	/* copy data */
	lis 3, __data_ro@h
	ori 3, 3, __data_ro@l
	lis 4, __data_rw@h
	ori 4, 4, __data_rw@l
	lis 5, __data_size@h
	ori 5, 5, __data_size@l
	bl memcpy

	/* zero bss */
	lis 3, __bss_start@h
	ori 3, 3, __bss_start@l
	mr 4, 0
	lis 5, __bss_size@h
	ori 5, 5, __bss_size@l
	bl memset

	/* jump to loader_main */
	b loader_main
