OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS
{
	. = 0x10000 + SIZEOF_HEADERS;

	.text : {
		*(.text)
		*(.glue_7)
		*(.glue_7t)
	} : text = 0xff

	. = ALIGN(4);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	. = ALIGN(0x1000);
	.data : {
		*(.data)
	} : data

	. = ALIGN(4);
	.bss .  : {
		*(.bss)
		*(COMMON)
	}

	/DISCARD/ : { *(.comment .note) }
}
