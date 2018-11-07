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
					uint8_t MSTKERR : 1;
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
	uint32_t HFSR;
	uint32_t DFSR;
	uint32_t MMFAR;
	uint32_t BFAR;
	uint32_t AFSR;
	uint8_t todo2[0x50];
};
static_assert(sizeof(struct scb) == 0x90, "Bad scb size!");
static struct scb *const SCB = (struct scb*)0xe000ed00;

/*
 * Cache and branch predictor maintenance operations
 */
struct cbp {
	uint32_t ICIALLU;
	uint32_t : 32;
	uint32_t ICIMVAU;
	uint32_t DCIMVAC;
	uint32_t DCISW;
	uint32_t DCCMVAU;
	uint32_t DCCMVAC;
	uint32_t DCCSW;
	uint32_t DCCIMVAC;
	uint32_t DCCISW;
	uint32_t BPIALL;
	uint32_t : 32;
	uint32_t : 32;
};
static_assert(sizeof(struct cbp) == 0x34, "Bad cbp size");
static struct cbp *const CBP = (struct cbp*)0xe000ef50;

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
static struct nvic *const NVIC = (struct nvic*)0xe000e100;

/*
 * FPU
 */
struct fpu {
	uint32_t FPCCR;
	uint32_t FPCAR;
	uint32_t FPDSCR;
	uint32_t MVFR0;
	uint32_t MVFR1;
	uint32_t MVFR2;
};
static_assert(sizeof(struct fpu) == 24, "Bad FPU size");
static struct fpu *const FPU = (struct fpu*)0xe000ef34;

/*
 * MPU
 */
union mpu_rbar {
	uint32_t r;
	struct {
		uint32_t REGION : 4;
		uint32_t VALID : 1;
		uint32_t ADDR : 27;
	};
};

union mpu_rasr {
	uint32_t r;
	struct {
		uint32_t ENABLE : 1;
		uint32_t SIZE : 5;
		uint32_t : 2;
		uint32_t SRD : 8;
		uint32_t B : 1;
		uint32_t C : 1;
		uint32_t S : 1;
		uint32_t TEX : 3;
		uint32_t : 2;
		enum {
			AP_None = 0,
			AP_Kern_RW = 1,
			AP_Kern_RW_User_RO = 2,
			AP_Kern_RW_User_RW = 3,
			AP_Kern_RO = 5,
			AP_Kern_RO_User_RO = 6,
		} AP : 3;
		uint32_t : 1;
		enum {
			XN_Execute = 0,
			XN_No_Execute = 1,
		} XN : 1;
		uint32_t : 3;
	};
};

struct mpu {
	union mpu_type {
		uint32_t r;
		struct {
			uint32_t SEPARATE : 1;
			uint32_t : 7;
			uint32_t DREGION : 8;
			uint32_t IREGION : 8;
			uint32_t : 8;
		};
	} TYPE;
	union mpu_ctrl {
		uint32_t r;
		struct {
			uint32_t ENABLE : 1;
			uint32_t HFNMIENA : 1;
			uint32_t PRIVDEFENA : 1;
			uint32_t : 29;
		};
	} CTRL;
	uint32_t RNR;
	union mpu_rbar RBAR;
	union mpu_rasr RASR;
	union mpu_rbar RBAR_A1;
	union mpu_rasr RASR_A1;
	union mpu_rbar RBAR_A2;
	union mpu_rasr RASR_A2;
	union mpu_rbar RBAR_A3;
	union mpu_rasr RASR_A3;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
};
static_assert(sizeof(struct mpu) == 0x60, "Bad MPU size");
static struct mpu *const MPU = (struct mpu*)0xe000ed90;

/*
 * values for 'flags' argument of mpu_init
 */
#define MPU_ENABLE_DEFAULT_MAP 0x1

/*
 * values for RASR register
 */
#define RASR_KERNEL_RWX_WBWA ((union mpu_rasr){ \
	.XN = XN_Execute, \
	.AP = AP_Kern_RW, \
	.TEX = 0b001, \
	.S = 0, \
	.C = 1, \
	.B = 1, \
}.r)
#define RASR_KERNEL_RW ((union mpu_rasr){ \
	.XN = XN_No_Execute, \
	.AP = AP_Kern_RW, \
	.TEX = 0b001, \
	.S = 0, \
	.C = 0, \
	.B = 0, \
}.r)
#define RASR_USER_R_WBWA ((union mpu_rasr){ \
	.XN = XN_No_Execute, \
	.AP = AP_Kern_RW_User_RO, \
	.TEX = 0b001, \
	.S = 0, \
	.C = 1, \
	.B = 1, \
}.r)
#define RASR_USER_RX_WBWA ((union mpu_rasr){ \
	.XN = XN_Execute, \
	.AP = AP_Kern_RW_User_RO, \
	.TEX = 0b001, \
	.S = 0, \
	.C = 1, \
	.B = 1, \
}.r)
#define RASR_USER_RW_WBWA ((union mpu_rasr){ \
	.XN = XN_No_Execute, \
	.AP = AP_Kern_RW_User_RW, \
	.TEX = 0b001, \
	.S = 0, \
	.C = 1, \
	.B = 1, \
}.r)
#define RASR_USER_RWX_WBWA ((union mpu_rasr){ \
	.XN = XN_Execute, \
	.AP = AP_Kern_RW_User_RW, \
	.TEX = 0b001, \
	.S = 0, \
	.C = 1, \
	.B = 1, \
}.r)

#endif /* !__ASSEMBLY__ */

#endif /* !arm_v7m_cpu_h */
