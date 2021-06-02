/*
 * machine_early_init.S - early machine initialisation
 */

#include <booke/asm_def.h>
#include <conf/config.h>
#include <e500v2/asm_def.h>

.text

/*
 * _start
 *
 * Kernel entry point
 *   r3-r6 contain arguments for kernel_start
 *
 * Major assumptions:
 *   CPU is in privileged state
 *   We are running from a phys=virt mapping in TLB1[0]
 *   The CPU is executing in translation space 0
 *
 * We need to:
 *   Configure machine state (MSR & HID registers)
 *   Switch to virtual address space
 */
.global _start
_start:
	/* Configure machine */
#define HID0_CONFIG (HID0_EN_MAS7_UPDATE | HID0_TBEN)
	lis 0, HID0_CONFIG@h
	ori 0, 0, HID0_CONFIG@l
	mtspr SPRN_HID0, 0

	/* Create a temporary mapping in TLB1[1] in translation space 1 */
	lis 0, (MAS0_TLBSEL(1) | MAS0_ESEL(0))@h
	mtspr SPRN_MAS0, 0
	tlbre					    /* read TLB1[0] */
	lis 0, (MAS0_TLBSEL(1) | MAS0_ESEL(1))@h
	mtspr SPRN_MAS0, 0
	mfspr 0, SPRN_MAS1
	ori 0, 0, MAS1_TS(1)			    /* change to TS1 */
	mtspr SPRN_MAS1, 0
	isync					    /* write TLB1[1] */
	tlbwe
	isync

	/* Switch to temporary mapping */
	lis 0, (.Ltmp - CONFIG_PAGE_OFFSET)@h
	ori 0, 0, (.Ltmp - CONFIG_PAGE_OFFSET)@l
	mtspr SPRN_SRR0, 0
	mfmsr 0
	ori 0, 0, (MSR_IS | MSR_DS)		    /* switch TS1 */
	mtspr SPRN_SRR1, 0
	rfi

.Ltmp:
	/* Create virt->phys mapping in TLB1[0] in translation space 0 */
	lis 0, (MAS0_TLBSEL(1) | MAS0_ESEL(0))@h
	mtspr SPRN_MAS0, 0
	tlbre					    /* read TLB1[0] */
	mfspr 0, SPRN_MAS2
	oris 0, 0, MAS2_EPN(PAGE_NUMBER_L(CONFIG_PAGE_OFFSET))@h
	mtspr SPRN_MAS2, 0			    /* write TLB1[0] */
	isync
	tlbwe
	isync

	/* Switch to translated mapping */
	lis 0, .Lvirt@h
	ori 0, 0, .Lvirt@l
	mtspr SPRN_SRR0, 0
	mfmsr 0
	rlwinm 0, 0, 0, ~(MSR_IS | MSR_DS)	    /* switch to TS0 */
	mtspr SPRN_SRR1, 0
	rfi

.Lvirt:
	/* Invalidate temporary mapping */
	lis 0, (MAS0_TLBSEL(1) | MAS0_ESEL(1))@h
	mtspr SPRN_MAS0, 0
	tlbre					    /* read TLB1[1] */
	li 0, 0
	mtspr SPRN_MAS1, 0			    /* invalidate */
	isync
	tlbwe					    /* write TLB1[1] */
	isync

	b kernel_start
