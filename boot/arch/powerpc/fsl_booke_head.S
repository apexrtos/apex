/*
 * fsl_booke_head.S
 *
 * Extra initialisation routines Freescale BookE processors.
 */

#include "booke_head.S"
#include <fsl_booke/asm_def.h>
#include <conf/config.h>

/*
 * fsl_booke_cache_init
 *
 * Stack is not available yet.
 */
.section .text.fsl_booke_cache_init
.global fsl_booke_cache_init
fsl_booke_cache_init:
	mflr 15
	bl fsl_booke_dcache_init
#if !defined(CONFIG_UNIFIED_CACHE)
	bl fsl_booke_icache_init
#endif
	mtlr 15
	blr

/*
 * fsl_booke_dcache_init
 */
.section .text.fsl_booke_dcache_init
fsl_booke_dcache_init:
	/* check if cache is already enabled */
	mfspr 3, SPRN_L1CSR0
	andi. 3, 3, L1CSR0_CE
	bnelr

	/* disable cache */
	msync
	isync
	li 3, 0
	mtspr SPRN_L1CSR0, 3
	msync
	isync

	/* invalidate cache */
	li 3, (L1CSR0_CFI | L1CSR0_CLFC)
	mtspr SPRN_L1CSR0, 3
	isync

	/* wait for completion */
1:	mfspr 3, SPRN_L1CSR0
	andi. 3, 3, (L1CSR0_CFI | L1CSR0_CLFC)
	bne 1b

	/* setup cache */
#define L1CSR0_CONFIG (L1CSR0_CECE | L1CSR0_CE)
	lis 3, L1CSR0_CONFIG@h
	ori 3, 3, L1CSR0_CONFIG@l
	msync
	isync
	mtspr SPRN_L1CSR0, 3
	isync
	blr

/*
 * fsl_booke_icache_init
 */
.section .text.fsl_booke_icache_init
fsl_booke_icache_init:
	/* check if cache is already enabled */
	mfspr 3, SPRN_L1CSR1
	andi. 3, 3, L1CSR1_ICE
	bnelr

	/* setup cache */
#define L1CSR1_CONFIG (L1CSR1_ICECE | L1CSR1_ICFI | L1CSR1_ICLFC | L1CSR1_ICE)
	lis 3, L1CSR1_CONFIG@h
	ori 3, 3, L1CSR1_CONFIG@l
	mtspr SPRN_L1CSR1, 3
	isync
	blr
