/*
 * Linker script for boot loader
 */

INCLUDE conf/config.ld

MEMORY {
	ro : ORIGIN = CONFIG_BOOTIMAGE_BASE_PHYS, LENGTH = CONFIG_BOOTIMAGE_SIZE
	rw : ORIGIN = CONFIG_LOADER_RAM_BASE_PHYS, LENGTH = CONFIG_LOADER_RAM_SIZE
}

SECTIONS {
	.text : {
		KEEP(*head.o(.text))
		*(.text*)
		. = ALIGN(4);
	} >ro

	.rodata : {
		*(.rodata*)
		. = ALIGN(4);
	} >ro

	.got2 : {
		*(.got2)
	} >ro

	.data : {
		*(.data*)
		. = ALIGN(4);
	} >rw AT>ro

	.bss : {
		*(.bss*)
		*(COMMON)
	} >rw

	.stack (NOLOAD) : ALIGN(8) {
		. += 0x1000;
	} >rw

	__data_ro = LOADADDR(.data);
	__data_rw = ADDR(.data);
	__data_size = SIZEOF(.data);
	__bss_start = ADDR(.bss);
	__bss_size = SIZEOF(.bss);
	__stack_top = ADDR(.stack) + SIZEOF(.stack);
	__loader_end = LOADADDR(.data) + SIZEOF(.data);

	/* REVISIT: llvm 12.0.0 ends up with eh_frame and eh_header sections
		    even though we're using -fno-exceptions? */
	/DISCARD/ : { *(.eh_*) }
}
