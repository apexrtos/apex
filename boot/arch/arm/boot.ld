/*
 * Linker script for boot loader
 */
INCLUDE conf/config.ld

MEMORY {
	rom : ORIGIN = CONFIG_ROM_BASE_PHYS, LENGTH = CONFIG_ROM_SIZE
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

	.got : {
		/* libc.a may be position independent */
		*(.got.plt)
		*(.igot.plt)
		*(.got)
		*(.igot)
	} >rom

	.data : {
		*(.data*)
	} >ram AT>rom

	.bss : {
		*(.bss*)
		*(COMMON)
	} >ram

	__data_rom = LOADADDR(.data);
	__data_ram = ADDR(.data);
	__data_size = SIZEOF(.data);
	__bss_start = ADDR(.bss);
	__bss_size = SIZEOF(.bss);
	__loader_end = LOADADDR(.data) + SIZEOF(.data);
}
