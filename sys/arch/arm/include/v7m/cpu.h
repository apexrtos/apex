#ifndef arm_v7m_cpu_h
#define arm_v7m_cpu_h

#include <conf/config.h>

/*
 * CONTROL register
 */
#define CONTROL_NPRIV	(1 << 0)    /* Thread mode has unprivileged access */
#define CONTROL_SPSEL	(1 << 1)    /* Thread mode uses SP_process stack */
#define CONTROL_FPCA	(1 << 2)    /* FP extension is active */

/*
 * EPSR register
 */
#define EPSR_T		(1 << 24)   /* Thumb mode */

/*
 * EXC_RETURN
 */
#if defined(CONFIG_FPU)
#define EXC_RETURN_USER	0xffffffed  /* Return to thread mode, process stack */
#define EXC_RETURN_KERN	0xffffffe1  /* Return to thread mode, main stack */
#else
#define EXC_RETURN_USER	0xfffffffd  /* Return to thread mode, process stack */
#define EXC_RETURN_KERN	0xfffffff1  /* Return to thread mode, main stack */
#endif
#define EXC_SPSEL	0x00000004  /* SPSEL when exception taken */

#ifndef __ASSEMBLY__

#include <assert.h>

/*
 * System Control Block
 */
struct scb {
	uint32_t CPUID;
	union scb_icsr {
		uint32_t r;
		struct {
			uint32_t VECTACTIVE : 9;
			uint32_t : 2;
			uint32_t RETTOBASE : 1;
			uint32_t VECTPENDING : 9;
			uint32_t : 1;
			uint32_t ISRPENDING : 1;
			uint32_t ISRPREEMPT : 1;
			uint32_t : 1;
			uint32_t PENDSTCLR : 1;
			uint32_t PENDSTSET : 1;
			uint32_t PENDSVCLR : 1;
			uint32_t PENDSVSET : 1;
			uint32_t : 2;
			uint32_t NMIPENDSET : 1;
		};
	} ICSR;
	uint8_t todo1[0x1c];
	union scb_shcsr {
		uint32_t r;
		struct {
			uint32_t MEMFAULTACT : 1;
			uint32_t BUSFAULTACT : 1;
			uint32_t : 1;
			uint32_t USGFAULTACT : 1;
			uint32_t : 3;
			uint32_t SVCALLACT : 1;
			uint32_t MONITORACT : 1;
			uint32_t : 1;
			uint32_t PENDSVACT : 1;
			uint32_t SYSTICKACT : 1;
			uint32_t USGFAULTPENDED : 1;
			uint32_t MEMFAULTPENDED : 1;
			uint32_t BUSFAULTPENDED : 1;
			uint32_t SVCALLPENDED : 1;
			uint32_t MEMFAULTENA : 1;
			uint32_t BUSFAULTENA : 1;
			uint32_t USGFAULTENA : 1;
			uint32_t : 13;
		};
	} SHCSR;
	union scb_cfsr {
		uint32_t r;
		struct {
			union scb_cfsr_mmfsr {
				uint8_t r;
				struct {
					uint8_t IACCVIOL : 1;
					uint8_t DACCVIOL : 1;
					uint8_t : 1;
					uint8_t MUNSTKERR : 1;
					uint8_t MTSKERR : 1;
					uint8_t MLSPERR : 1;
					uint8_t : 1;
					uint8_t MMARVALID : 1;
				};
			} MMFSR;
			union scb_cfsr_bfsr {
				uint8_t r;
				struct {
					uint8_t IBUSERR : 1;
					uint8_t PRECISERR : 1;
					uint8_t IMPRECISERR : 1;
					uint8_t UNSTKERR : 1;
					uint8_t STKERR : 1;
					uint8_t LSPERR : 1;
					uint8_t : 1;
					uint8_t BFARVALID : 1;
				};
			} BFSR;
			union scb_cfsr_ufsr {
				uint16_t r;
				struct {
					uint16_t UNDEFINSTR : 1;
					uint16_t INVSTATE : 1;
					uint16_t INVPC : 1;
					uint16_t NOCP : 1;
					uint16_t : 4;
					uint16_t UNALIGNED : 1;
					uint16_t DIVBYZERO : 1;
					uint16_t : 6;
				};
			} UFSR;
		};
	} CFSR;
	uint8_t todo2[0x64];
};
static_assert(sizeof(struct scb) == 0x90, "Bad scb size!");
static volatile struct scb *const SCB = (struct scb*)0xe000ed00;

/*
 * SysTick
 */
struct syst {
	union syst_csr {
		uint32_t r;
		struct {
			unsigned ENABLE : 1;
			unsigned TICKINT : 1;
			unsigned CLKSOURCE : 1;
			unsigned : 13;
			unsigned COUNTFLAG : 1;
		};
	} CSR;
	uint32_t RVR;
	uint32_t CVR;
	union syst_calib {
		uint32_t r;
		struct {
			unsigned TENMS : 24;
			unsigned : 6;
			unsigned SKEW : 1;
			unsigned NOREF : 1;
		};
	} CALIB;
};
static_assert(sizeof(struct syst) == 16, "Bad SYST size");
static volatile struct syst *const SYST = (struct syst*)0xe000e010;

/*
 * NVIC
 */
struct nvic {
	uint32_t ISER[16];
	uint32_t reserved_0[16];
	uint32_t ICER[16];
	uint32_t reserved_1[16];
	uint32_t ISPR[16];
	uint32_t reserved_2[16];
	uint32_t ICPR[16];
	uint32_t reserved_3[16];
	uint32_t IABR[16];
	uint32_t reserved_4[48];
	uint8_t IPR[496];
	uint32_t reserved_5[452];
};
static_assert(sizeof(struct nvic) == 3072, "Bad NVIC size");
static volatile struct nvic *const NVIC = (struct nvic*)0xe000e100;

#endif /* !__ASSEMBLY__ */

#endif /* !arm_v7m_cpu_h */
