/*
 * machine_init.S
 *
 * Machine initialisation for QEMU ppce500.
 */

.text

#include <booke/asm_def.h>
#include <conf/config.h>

/*
 * machine_init
 *
 * Stack is not available yet.
 */
.global machine_init
machine_init:
	mflr 14

	/* no clock initialisation required */
	/* no cache initialisation required */

	/* QEMU sets TLB1[0] to a mapping covering only the boot loader image.
	 * Modify this to cover the first 1GiB of the address space.
	 * This allows the kernel to be loaded anywhere in the first 1GiB */
#define MAS0_TLB0 (MAS0_TLBSEL(1) | MAS0_ESEL(0))
#define MAS1_TLB0 (MAS1_V | MAS1_IPROT | MAS1_TSIZE(20))
#define MAS2_TLB0 (MAS2_EPN(PAGE_NUMBER_L(CONFIG_RAM_BASE_PHYS)))
#define MAS3_TLB0 (MAS3_RPN(PAGE_NUMBER_L(CONFIG_RAM_BASE_PHYS)) | MAS3_SX | MAS3_SW | MAS3_SR)
#define MAS7_TLB0 (MAS7_RPNU(PAGE_NUMBER_U(CONFIG_RAM_BASE_PHYS)))
	lis 3, MAS0_TLB0@h
	ori 3, 3, MAS0_TLB0@l
	mtspr SPRN_MAS0, 3
	lis 3, MAS1_TLB0@h
	ori 3, 3, MAS1_TLB0@l
	mtspr SPRN_MAS1, 3
	lis 3, MAS2_TLB0@h
	ori 3, 3, MAS2_TLB0@l
	mtspr SPRN_MAS2, 3
	lis 3, MAS3_TLB0@h
	ori 3, 3, MAS3_TLB0@l
	mtspr SPRN_MAS3, 3
	lis 3, MAS7_TLB0@h
	ori 3, 3, MAS7_TLB0@l
	mtspr SPRN_MAS7, 3
	isync
	tlbwe
	isync

	/* Map NS16550 UART at 0xfe0004500 to 0xfe004500 */
#if defined(CONFIG_BOOT_CONSOLE)
#define MAS0_TLB1 (MAS0_TLBSEL(1) | MAS0_ESEL(1))
#define MAS1_TLB1 (MAS1_V | MAS1_TSIZE(2))
#define MAS2_TLB1 (MAS2_EPN(PAGE_NUMBER_L(0xfe004000)) | MAS2_I | MAS2_G)
#define MAS3_TLB1 (MAS3_RPN(PAGE_NUMBER_L(0xfe0004000)) | MAS3_SW | MAS3_SR)
#define MAS7_TLB1 (MAS7_RPNU(PAGE_NUMBER_U(0xfe0004000)))
	lis 3, MAS0_TLB1@h
	ori 3, 3, MAS0_TLB1@l
	mtspr SPRN_MAS0, 3
	lis 3, MAS1_TLB1@h
	ori 3, 3, MAS1_TLB1@l
	mtspr SPRN_MAS1, 3
	lis 3, MAS2_TLB1@h
	ori 3, 3, MAS2_TLB1@l
	mtspr SPRN_MAS2, 3
	lis 3, MAS3_TLB1@h
	ori 3, 3, MAS3_TLB1@l
	mtspr SPRN_MAS3, 3
	lis 3, MAS7_TLB1@h
	ori 3, 3, MAS7_TLB1@l
	mtspr SPRN_MAS7, 3
	isync
	tlbwe
	isync
#endif

	mtlr 14
	blr

/*
 * machine_exception
 */
.global machine_exception
machine_exception:
	b .
