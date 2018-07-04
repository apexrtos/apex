/*
 * Linker script for kernel
 */

INCLUDE conf/config.ld

ENTRY(_start)

PHDRS {
	hdrs PT_PHDR FILEHDR PHDRS;
	kernel PT_LOAD;
}

MEMORY {
	ram : ORIGIN = CONFIG_KERNEL_BASE_VIRT, LENGTH = CONFIG_KERNEL_MAX_SIZE
}

SECTIONS {
	.text : {
		*(.vectors)
		*(.text*)
	} >ram : kernel

	.rodata : {
		*(.rodata*)
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

	__stack_start = ADDR(.stack);
	__stack_size = SIZEOF(.stack);
	__stack_top = ADDR(.stack) + SIZEOF(.stack);

	/* Discard .ARM.exidx and .ARM.attributes */
	/DISCARD/ : { *(.ARM.*) }
}
