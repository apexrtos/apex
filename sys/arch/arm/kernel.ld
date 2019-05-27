/*
 * Linker script for kernel
 */

INCLUDE conf/config.ld

ENTRY(_start)

PHDRS {
	kernel PT_LOAD FILEHDR PHDRS;
}

MEMORY {
	ram : ORIGIN = CONFIG_KERNEL_BASE_VIRT, LENGTH = CONFIG_KERNEL_MAX_SIZE
}

SECTIONS {
	.text ORIGIN(ram) + SIZEOF_HEADERS : {
		*(.vectors)
		*(.text*)
		*(.fast_text)
	} >ram : kernel

	.rodata : {
		*(.rodata*)
		*(.fast_rodata)
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
		*(.fast_bss)
		*(COMMON)
	} >ram : kernel

	.stack : ALIGN(8) {
		. += CONFIG_KSTACK_SIZE;
	} >ram : kernel

	__elf_headers = ORIGIN(ram);
	__init_array_start = ADDR(.init_array);
	__init_array_end = ADDR(.init_array) + SIZEOF(.init_array);
	__stack_start = ADDR(.stack);
	__stack_size = SIZEOF(.stack);
	__stack_top = ADDR(.stack) + SIZEOF(.stack);

	/* Discard .ARM.exidx and .ARM.attributes */
	/DISCARD/ : { *(.ARM.*) }
}
