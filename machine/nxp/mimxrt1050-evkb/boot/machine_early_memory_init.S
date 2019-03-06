/*
 * machine_early_memory_init.S
 */

 #include <conf/config.h>

.syntax unified

#define ITCM_BANKS (CONFIG_ITCM_SIZE / 0x8000)
#define DTCM_BANKS (CONFIG_DTCM_SIZE / 0x8000)

/*
 * Some IMXRT processors contain a second on chip SRAM. We prefer to use this
 * for DMA memory instead of flexram to increase the size of available DTCM.
 */
#if defined(CONFIG_SRAM_SIZE)
#define DMA_BANKS 0
#else
#define DMA_BANKS (CONFIG_DMA_SIZE / 0x8000)
#endif

#define ITCM_CONFIG (0xffffffff & (0xffffffff << (32 - ITCM_BANKS << 1)))
#define DTCM_CONFIG (0xaaaaaaaa & (0xffffffff >> (32 - DTCM_BANKS << 1) << (DMA_BANKS << 1)))
#define OCRAM_CONFIG (0x55555555 & (0xffffffff >> (32 - DMA_BANKS << 1)))

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
	ldr r0, =(ITCM_CONFIG + DTCM_CONFIG + OCRAM_CONFIG)
	ldr r1, =0x400ac044
	str r0, [r1]

	/* set FLEXRAM_BANK_CFG_SEL */
	ldr r0, =0x00200007
	ldr r1, =0x400ac040
	str r0, [r1]

	bx lr
