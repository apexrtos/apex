/*
 * Linker script for IMXRT10xx boot from flexspi headers
 */
INCLUDE conf/config.ld

MEMORY {
	rom : ORIGIN = CONFIG_ROM_BASE_PHYS, LENGTH = CONFIG_ROM_SIZE
}

SECTIONS {
	.headers : {
		*config.o(.rodata)	/* config must be at offset 0x0 */
		boot_data = .;
		*boot_data.o(.rodata)
		dcd = .;
		*dcd.o(.rodata)
		. = 0x1000;		/* IVT must be at offset 0x1000 */
		ivt = .;
		*ivt.o(.rodata)
		. = CONFIG_LOADER_OFFSET;
	} > rom = 0xff
	entry = .;

	/* lld complains if _start is not set */
	_start = 0;

	/* lld complains when no memory region is
	 * specified for a section even if it's empty */
	/DISCARD/ : { *(.text*) }
}
