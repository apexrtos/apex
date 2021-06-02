/*
 * Linker script for kernel
 */

INCLUDE conf/config.ld

PHDRS {
	kernel PT_LOAD FILEHDR PHDRS;
}

MEMORY {
	ram : ORIGIN = CONFIG_KERNEL_BASE_VIRT, LENGTH = CONFIG_KERNEL_MAX_SIZE
}

SECTIONS {
	.text ORIGIN(ram) + SIZEOF_HEADERS : {
		*(.fast_text)
		*(.text*)
	} >ram : kernel

	.rodata : {
		*(.fast_rodata)
		*(.rodata*)
	} >ram : kernel

	.got2 : {
		*(.got2)
	} >ram : kernel

	.init_array : {
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP (*(.init_array))
	} >ram : kernel

	.filesystems : {
		__filesystems = .;
		KEEP(*(.filesystems))
		__filesystems_end = .;
	} >ram : kernel

	.data : {
		*(.data*)
		*(.fast_data)
	} >ram : kernel

	.bss : {
		*(.bss*)
		*(COMMON)
	} >ram : kernel

	.fast_bss (NOLOAD) : {
		*(.fast_bss)
	} >ram : kernel

	.stack (NOLOAD) : ALIGN(16) {
		. += CONFIG_KSTACK_SIZE;
	} >ram : kernel

	__elf_headers = ORIGIN(ram);
	__init_array_start = ADDR(.init_array);
	__init_array_end = ADDR(.init_array) + SIZEOF(.init_array);
	__stack_start = ADDR(.stack);
	__stack_size = SIZEOF(.stack);
	__stack = ADDR(.stack) + SIZEOF(.stack);

	/* REVISIT: llvm 12.0.0 ends up with eh_frame and eh_header sections
		    even though we're using -fno-exceptions? */
	/DISCARD/ : { *(.eh_*) }
}
