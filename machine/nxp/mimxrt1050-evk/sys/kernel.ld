/*
 * Linker script for kernel running on MIMXRT1050-EVK
 */

INCLUDE conf/config.ld

ENTRY(_start)

PHDRS {
	hdrs PT_PHDR FILEHDR PHDRS;
	dram PT_LOAD;
	itcm PT_LOAD;
	dtcm PT_LOAD;
}

MEMORY {
	dram : ORIGIN = CONFIG_DRAM_BASE_VIRT, LENGTH = CONFIG_DRAM_SIZE
	itcm : ORIGIN = CONFIG_ITCM_BASE_VIRT, LENGTH = CONFIG_ITCM_SIZE
	dtcm : ORIGIN = CONFIG_DTCM_BASE_VIRT, LENGTH = (CONFIG_DTCM_SIZE - CONFIG_LOADER_RAM_SIZE)
}

SECTIONS {
	.fast_text : {
		*(.vectors)
		*(.fast_text)
	} >itcm : itcm

	.text : {
		*(.text*)
	} >dram : dram

	.rodata : {
		*(.rodata*)
	} >dram : dram

	.filesystems : {
		__filesystems = .;
		KEEP(*(.filesystems))
		__filesystems_end = .;
	} >dram : dram

	.data : {
		*(.data*)
	} >dram : dram

	.bss : {
		*(.bss*)
		*(COMMON)
	} >dram : dram

	.stack : ALIGN(8) {
		. += CONFIG_KSTACK_SIZE;
	} >dram : dram

	.fast_data : {
		*(.fast_data)
		*(.fast_rodata)
	} >dtcm : dtcm

	 .fast_bss : {
		*(.fast_bss)
	} >dtcm : dtcm

	__stack_start = ADDR(.stack);
	__stack_size = SIZEOF(.stack);
	__stack_top = ADDR(.stack) + SIZEOF(.stack);

	/* Discard .ARM.exidx and .ARM.attributes */
	/DISCARD/ : { *(.ARM.*) }
}
