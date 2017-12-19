/*
 * Linker script for driver
 */
INCLUDE config.ld
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(driver_start)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS
{
	. = DRIVER_BASE + SIZEOF_HEADERS ;

	.text : {
		*(.text)
	} : text = 0x9090

	. = ALIGN(32);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	. = ALIGN(32);
	.data : {
		*(.data)
	} : data

	.bss . : {
		*(.bss)
		*(COMMON)
	}
	/DISCARD/ : { *(.comment .note) }
}
