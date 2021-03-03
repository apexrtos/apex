#pragma once

/*
 * System Control Space Base Address
 */
#define A_SCS	    0xe000e000

/*
 * System Control Block
 */
#define A_CPUID	    0xe000ed00  /* CPUID Base Register */
#define A_ICSR	    0xe000ed04  /* Interrupt Control and State Register */
#define A_VTOR	    0xe000ed08  /* Vector Table Offset Register */
#define A_AIRCR	    0xe000ed0c  /* Application Interrupt and Reset Control Register */
#define A_SCR	    0xe000ed10  /* System Control Register */
#define A_CCR	    0xe000ed14  /* Configuration and Control Register */
#define A_SHPR1	    0xe000ed18  /* System Handler Priority Register 1 */
#define A_SHPR2	    0xe000ed1c  /* System Handler Priority Register 2 */
#define A_SHPR3	    0xe000ed20  /* System Handler Priority Register 3 */
#define A_SHCSR	    0xe000ed24  /* System Handler Control and State Register */
#define A_CFSR	    0xe000ed28  /* Configurable Fault Status Register */
#define A_HFSR	    0xe000ed2c  /* HardFault Status Register */
#define A_DFSR	    0xe000ed30  /* Debug Fault Status Register */
#define A_MMFAR	    0xe000ed34  /* MemManage Fault Address Register */
#define A_BFAR	    0xe000ed38  /* BusFault Address Register */
#define A_AFSR	    0xe000ed3c  /* Auxiliary Fault Status Register */
#define A_CPACR	    0xe000ed88  /* Coprocessor Access Control Register */
#define A_FPCCR	    0xe000ef34  /* Floating Point Context Control Register */
#define A_FPCAR	    0xe000ef38  /* Floating Point Context Address Register */
#define A_FPDSCR    0xe000ef3c  /* Floating Point Default Status Control Register */
#define A_MVFR0	    0xe000ef40  /* Media and FP Feature Register 0 */
#define A_MVFR1	    0xe000ef44  /* Media and FP Feature Register 1 */
#define A_MVFR2	    0xe000ef48  /* Media and FP Feature Register 2 */

/*
 * Configuration and Control Register
 */
#define CCR_BP		    (1 << 18)	/* Branch prediction enable */
#define CCR_IC		    (1 << 17)	/* Instruction cache enable */
#define CCR_DC		    (1 << 16)   /* Cache enable */
#define CCR_STKALIGN	    (1 << 9)    /* Align stack to 8 bytes on exception */
#define CCR_BFHFNMIGN	    (1 << 8)	/* Ignore faults in critical handlers */
#define CCR_DIV_0_TRP	    (1 << 4)	/* Trap divide by zero */
#define CCR_UNALIGN_TRP	    (1 << 3)	/* Trap unaligned word or halfword access */
#define CCR_USERSETMPEND    (1 << 1)	/* Allow unprivileged writes to STIR */
#define CCR_NONBASETHRDENA  (1 << 0)	/* Allow thread mode with active exception */

/*
 * System Handler Control and State Register
 */
#define SHCSR_USGFAULTNA    (1 << 18)
#define SHCSR_BUSFAULTENA   (1 << 17)
#define SHCSR_MEMFAULTENA   (1 << 16)

/*
 * Floating Point Context Control Register
 */
#define FPCCR_ASPEN	(1 << 31)   /* Set CONTROL.FPCA on FP instruction */
#define FPCCR_LSPEN	(1 << 30)   /* Enable lazy context save of FP state */

/*
 * System Control Space
 */
#define A_ID_PFR0   0xe000ed40	/* Processor Feature Register 0 */
#define A_ID_PFR1   0xe000ed44	/* Processor Feature Register 1 */
#define A_ID_DFR0   0xe000ed48	/* Debug Feature Register 0 */
#define A_ID_AFR0   0xe000ed4c	/* Auxiliary Feature Register 0 */
#define A_ID_MMFR0  0xe000ed50	/* Memory Model Feature Register 0 */
#define A_ID_MMFR1  0xe000ed54	/* Memory Model Feature Register 1 */
#define A_ID_MMFR2  0xe000ed58	/* Memory Model Feature Register 2 */
#define A_ID_MMFR3  0xe000ed5c	/* Memory Model Feature Register 3 */
#define A_ID_ISAR0  0xe000ed60	/* Instruction Set Attribute Register 0 */
#define A_ID_ISAR1  0xe000ed64	/* Instruction Set Attribute Register 1 */
#define A_ID_ISAR2  0xe000ed68	/* Instruction Set Attribute Register 2 */
#define A_ID_ISAR3  0xe000ed6c	/* Instruction Set Attribute Register 3 */
#define A_ID_ISAR4  0xe000ed70	/* Instruction Set Attribute Register 4 */
#define A_ID_ISAR5  0xe000ed74	/* Reserved, RAZ */
#define A_CLIDR	    0xe000ed78	/* Cache Level ID Register */
#define A_CTR	    0xe000ed7c	/* Cache Type Register */
#define A_CCSIDR    0xe000ed80	/* Cache Size ID Registers */
#define A_CSSELR    0xe000ed84	/* Cache Size Selection Register */

/*
 * Cache and branch predictor maintenance operations
 */
#define A_ICIALLU   0xe000ef50  /* I-cache invalidate all to PoU */
#define A_ICIMVAU   0xe000ef58  /* I-cache invalidate by MVA to PoU */
#define A_DCIMVAC   0xe000ef5c  /* D-cache invalidate by MVA to PoC */
#define A_DCISW	    0xe000ef60  /* D-cache invalidate by set-way */
#define A_DCCMVAU   0xe000ef64  /* D-cache clean by MVA to PoU */
#define A_DCCMVAC   0xe000ef68  /* D-cache clean by MVA to PoC */
#define A_DCCSW	    0xe000ef6c  /* D-cache clean by set-way */
#define A_DCCIMVAC  0xe000ef70  /* D-cache clean and invalidate by MVA to PoC */
#define A_DCCISW    0xe000ef74  /* D-cache clean and invalidate by set-way */
#define A_BPIALL    0xe000ef78  /* Branch predictor invalidate all */
