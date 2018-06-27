/*
 * crt_init.s
 *
 * enough assembly to run loader_main
 */

.syntax unified

/*
 * crt_init
 */
.text
#ifdef __thumb__
.thumb_func
#endif
.global crt_init
crt_init:
	/* copy data */
	ldr r0, =__data_ram
	ldr r1, =__data_rom
	ldr r2, =__data_size
	bl memcpy

	/* zero bss */
	ldr r0, =__bss_start
	mov r1, 0
	ldr r2, =__bss_size
	bl memset

	/* jump to loader_main */
	bl loader_main
.ltorg
