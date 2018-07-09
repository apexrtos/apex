/*
 * machine_early_memory_init.S
 */

 #include <conf/config.h>

.syntax unified

#define ITCM_BANKS (CONFIG_ITCM_SIZE / 0x8000)
#define DTCM_BANKS (CONFIG_DTCM_SIZE / 0x8000)

/*
 * machine_early_memory_init
 *
 * Configure FlexRAM ITCM/DTCM
 */
.text
.thumb_func
.global machine_early_memory_init
machine_early_memory_init:
	/* set FLEXRAM_BANK_CFG */
	ldr r0, =((0xffffffff & (0xffffffff << (32 - ITCM_BANKS << 1))) + (0xaaaaaaaa & (0xffffffff >> (ITCM_BANKS << 1))))
	ldr r1, =0x400ac044
	str r0, [r1]

	/* set FLEXRAM_BANK_CFG_SEL */
	ldr r0, =0x00200007
	ldr r1, =0x400ac040
	str r0, [r1]

	bx lr
