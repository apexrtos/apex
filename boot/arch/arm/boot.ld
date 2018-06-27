/*
 * Linker script for boot loader
 */
INCLUDE conf/config.ld

MEMORY {
	rom : ORIGIN = CONFIG_ROM_BASE_PHYS + CONFIG_ROM_OFFSET, LENGTH = CONFIG_ROM_SIZE - CONFIG_ROM_OFFSET
	ram : ORIGIN = CONFIG_LOADER_RAM_BASE_PHYS, LENGTH = CONFIG_LOADER_RAM_SIZE
}

SECTIONS {
	.text : {
		KEEP(*head.o(.text))
		*(.text*)
	} >rom

	.rodata : {
		*(.rodata*)
	} >rom

	.data : {
		*(.data*)
	} >ram AT>rom

	.bss : {
		*(.bss*)
		*(COMMON)
	} >ram

	.stack : ALIGN(8) {
		. += 0x1000;
	} >ram

	__data_rom = LOADADDR(.data);
	__data_ram = ADDR(.data);
	__data_size = SIZEOF(.data);
	__bss_start = ADDR(.bss);
	__bss_size = SIZEOF(.bss);
	__stack_top = ADDR(.stack) + SIZEOF(.stack);
	__loader_end = LOADADDR(.data) + SIZEOF(.data);
}
