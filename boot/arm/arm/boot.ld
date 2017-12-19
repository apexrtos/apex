/*
 * Linker script for boot loader
 */
INCLUDE config.ld

SECTIONS {
	. = LOADER_BASE ;

	.text . : {
		*(.text)
	}
	.rodata . : {
		*(.rodata)
	}
	.data . : {
		*(.data)
	}
	.bss . : {
		*(.bss)
		*(COMMON)
	}
	. = LOADER_BASE + 0x1fff;
	.tail : {
		*(.tail)
	}
}
