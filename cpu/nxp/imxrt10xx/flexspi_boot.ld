/*
 * Linker script for IMXRT10xx boot from flash headers
 */
INCLUDE conf/config.ld

MEMORY {
	rom : ORIGIN = CONFIG_ROM_BASE_PHYS, LENGTH = CONFIG_ROM_SIZE
}

SECTIONS {
	.headers : {
		*config.o(.rodata)	/* config must be at offset 0x0 */
		*boot_data.o(.rodata)
		dcd = .;
		*dcd.o(.rodata)
		. = 0x1000;		/* IVT must be at offset 0x1000 */
		*ivt.o(.rodata)
		. = CONFIG_ROM_OFFSET;
	} > rom = 0xff
	entry = .;
}
