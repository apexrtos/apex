/*
 * Linker script for IMXRT10xx boot from usdhc headers
 */
INCLUDE conf/config.ld

MEMORY {
	/* Initial load region is 4KiB */
	rom : ORIGIN = 0, LENGTH = 4096
}

SECTIONS {
	.headers : {
		. = 0x400;		/* IVT must be at offset 0x400 */
		ivt = . + CONFIG_LOAD_REGION_BASE_PHYS;
		*ivt.o(.rodata)
		boot_data = . + CONFIG_LOAD_REGION_BASE_PHYS;
		*boot_data.o(.rodata)
		dcd = . + CONFIG_LOAD_REGION_BASE_PHYS;
		*dcd.o(.rodata)
		. = CONFIG_LOADER_OFFSET;
	} > rom
	entry = . + CONFIG_LOAD_REGION_BASE_PHYS;

	/* lld complains if _start is not set */
	_start = 0;

	/* lld complains when no memory region is
	 * specified for a section even if it's empty */
	/DISCARD/ : { *(.text*) }
}
