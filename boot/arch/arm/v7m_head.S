/*
 * head.S
 *
 * set stack pointer and jump to crt_init
 */

#include <conf/config.h>
#include <sys/arch/arm/include/v7m/asm.h>

.syntax unified

/*
 * Interrupt Vector Table
 */
.text
.global vectors
vectors:
	.word __stack_top
	.word _start
	.word boot_isr_NMI
	.word boot_isr_HardFault
	.word boot_isr_MemManage
	.word boot_isr_BusFault
	.word boot_isr_UsageFault
	.word 0				/* 0x1c Reserved */
	.word 0				/* 0x20 Reserved */
	.word 0				/* 0x24 Reserved */
	.word 0				/* 0x28 Reserved */
	.word boot_isr_SVCall
	.word boot_isr_DebugMonitor
	.word 0				/* 0x34 Reserved */
	.word boot_isr_PendSV
	.word boot_isr_SysTick
	.rept CONFIG_IRQS
	.word boot_isr_NVIC
	.endr

/*
 * start
 */
.text
.thumb_func
.global _start
_start:
	/* ARMv7-M starts in Thumb, Thread, Privileged mode using main stack
	   with interrupts enabled */

	/* disable interrupts */
	cpsid	i

	/* initialise clocks */
	bl machine_clock_init

	mov r11, A_SCS

	/* initialise cache */
#if defined(CONFIG_CACHE) && !defined(CONFIG_NO_CACHE_INIT)
	/* originally from ARM Cortex-M7 Devices Generic User Guide Issue B */
	/* data cache */
	mov r0, 0	    /* select level 1 data cache */
	str r0, [r11, A_CSSELR-A_SCS]
	dsb		    /* Ensure write to CSSELR before proceeding */
	ldr r2, [r11, A_CCSIDR-A_SCS] /* Read data cache size information from CCSIDR */
	and r1, r2, 7	    /* r1 = cache line size */
	add r7, r1, 4	    /* r7 = number of words in a cache line */
	ubfx r4, r2, 3, 10  /* r4 = number of ways-1 of data cache */
	ubfx r2, r2, 13, 15 /* r2 = number of sets-1 of data cache */
	clz r6, r4	    /* calculate bit offset for way in DCISW8 */
1:			    /* for each set*/
	mov r1, r4	    /* r1 = number of ways-1 */
	lsls r8, r2, r7	    /* shift set value to bit 5 of r8 */
2:			    /* for each way */
	lsls r3, r1, r6	    /* shift way value to bit 30 in r6 */
	orrs r3, r3, r8	    /* merge way and set value for DCISW */
	str r3, [r11, A_DCISW-A_SCS] /* invalidate D-cache line */
	subs r1, r1, 1	    /* decrement way */
	bge 2b		    /* loop for each way */
	subs r2, r2, 1	    /* Decrement set */
	bge 1b		    /* loop for each set */

	/* instruction cache */
	mov r0, 0
	str r0, [r11, A_ICIALLU-A_SCS]

	dsb
	isb
#endif

	/* enable branch predictor, instruction cache, data cache and 8 byte
	 * stack alignment on exception */
	movw r0, :lower16:(CCR_BP|CCR_IC|CCR_DC|CCR_STKALIGN)
	movt r0, :upper16:(CCR_BP|CCR_IC|CCR_DC|CCR_STKALIGN)
	str r0, [r11, A_CCR-A_SCS]
	dsb
	isb

	/* jump to crt_init */
	b crt_init
.ltorg

/*
 * Default interrupt service routines
 */
#define BOOT_ISR(ISR) \
.text; \
.thumb_func; \
.global boot_isr_##ISR; \
.weak boot_isr_##ISR; \
boot_isr_##ISR: \
	bkpt; \
1:	b 1b;

BOOT_ISR(NMI)
BOOT_ISR(HardFault)
BOOT_ISR(MemManage)
BOOT_ISR(BusFault)
BOOT_ISR(UsageFault)
BOOT_ISR(SVCall)
BOOT_ISR(DebugMonitor)
BOOT_ISR(PendSV)
BOOT_ISR(SysTick)
BOOT_ISR(NVIC)

