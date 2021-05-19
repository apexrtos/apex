#pragma once

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
#define EPSR_ICI_IT	0x0600fc00  /* If-Then/Instruction-Continuation */
#define XPSR_FRAMEPTRALIGN (1 << 9) /* Exception aligned stack to 8 bytes */

/*
 * EXC_RETURN
 */
#define EXC_RETURN_HANDLER_MAIN_EXTENDED    0xffffffe1
#define EXC_RETURN_THREAD_MAIN_EXTENDED	    0xffffffe9
#define EXC_RETURN_THREAD_PROCESS_EXTENDED  0xffffffed
#define EXC_RETURN_HANDLER_MAIN_BASIC	    0xfffffff1
#define EXC_RETURN_THREAD_MAIN_BASIC	    0xfffffff9
#define EXC_RETURN_THREAD_PROCESS_BASIC	    0xfffffffd
#define EXC_SPSEL	0x00000004  /* exception on process stack */
#define EXC_THREAD	0x00000008  /* exception from thread mode */
#define EXC_NOT_FPCA	0x00000010  /* return with out FP extension */

#ifndef __ASSEMBLY__

#include "bitfield.h"
#include <cassert>
#include <cstdint>

/*
 * System Control Block
 */
struct scb {
	uint32_t CPUID;
	union icsr {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbit<S, bool, 31> NMIPENDSET;
		bitfield::armbit<S, bool, 28> PENDSVSET;
		bitfield::armbit<S, bool, 26> PENDSTSET;
		bitfield::armbit<S, bool, 25> PENDSTCLR;
		bitfield::armbit<S, bool, 23> ISRPREEMPT;
		bitfield::armbit<S, bool, 22> ISRPENDING;
		bitfield::armbits<S, unsigned, 20, 12> VECTPENDING;
		bitfield::armbit<S, bool, 11> RETTOBASE;
		bitfield::armbits<S, unsigned, 8, 0> VECTACTIVE;
	} ICSR;
	uint32_t VTOR;
	union aircr {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbits<S, unsigned, 31, 16> VECTKEY;
		bitfield::armbit<S, bool, 15> ENDIANNESS;
		bitfield::armbits<S, unsigned, 10, 8> PRIGROUP;
		bitfield::armbit<S, bool, 2> SYSRESETREQ;
		bitfield::armbit<S, bool, 1> VECTCLRACTIVE;
		bitfield::armbit<S, bool, 0> VECTRESET;
	} AIRCR;
	uint32_t SCR;
	uint32_t CCR;
	uint32_t SHPR1;
	uint32_t SHPR2;
	uint32_t SHPR3;
	union shcsr {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbit<S, bool, 18> USGFAULTENA;
		bitfield::armbit<S, bool, 17> BUSFAULTENA;
		bitfield::armbit<S, bool, 16> MEMFAULTENA;
		bitfield::armbit<S, bool, 15> SVCALLPENDED;
		bitfield::armbit<S, bool, 14> BUSFAULTPENDED;
		bitfield::armbit<S, bool, 13> MEMFAULTPENDED;
		bitfield::armbit<S, bool, 12> USGFAULTPENDED;
		bitfield::armbit<S, bool, 11> SYSTICKACT;
		bitfield::armbit<S, bool, 10> PENDSVACT;
		bitfield::armbit<S, bool, 8> MONITORACT;
		bitfield::armbit<S, bool, 7> SVCALLACT;
		bitfield::armbit<S, bool, 3> USGFAULTACT;
		bitfield::armbit<S, bool, 1> BUSFAULTACT;
		bitfield::armbit<S, bool, 0> MEMFAULTACT;
	} SHCSR;
	struct cfsr {
		union mmfsr {
			using S = uint8_t;
			struct { S r; };
			bitfield::armbit<S, bool, 7> MMARVALID;
			bitfield::armbit<S, bool, 5> MLSPERR;
			bitfield::armbit<S, bool, 4> MSTKERR;
			bitfield::armbit<S, bool, 3> MUNSTKERR;
			bitfield::armbit<S, bool, 1> DACCVIOL;
			bitfield::armbit<S, bool, 0> IACCVIOL;
		} MMFSR;
		union bfsr {
			using S = uint8_t;
			struct { S r; };
			bitfield::armbit<S, bool, 7> BFARVALID;
			bitfield::armbit<S, bool, 5> LSPERR;
			bitfield::armbit<S, bool, 4> STKERR;
			bitfield::armbit<S, bool, 3> UNSTKERR;
			bitfield::armbit<S, bool, 2> IMPRECISERR;
			bitfield::armbit<S, bool, 1> PRECISERR;
			bitfield::armbit<S, bool, 0> IBUSERR;
		} BFSR;
		union ufsr {
			using S = uint16_t;
			struct { S r; };
			bitfield::armbit<S, bool, 9> DIVBYZERO;
			bitfield::armbit<S, bool, 8> UNALIGNED;
			bitfield::armbit<S, bool, 3> NOCP;
			bitfield::armbit<S, bool, 2> INVPC;
			bitfield::armbit<S, bool, 1> INVSTATE;
			bitfield::armbit<S, bool, 0> UNDEFINSTR;
		} UFSR;
	} CFSR;
	uint32_t HFSR;
	uint32_t DFSR;
	uint32_t MMFAR;
	uint32_t BFAR;
	uint32_t AFSR;
	uint8_t todo2[0x50];
};
static_assert(sizeof(scb) == 0x90, "Bad scb size!");
static scb *const SCB = (scb*)0xe000ed00;

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
static_assert(sizeof(cbp) == 0x34, "Bad cbp size");
static cbp *const CBP = (cbp*)0xe000ef50;

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
static_assert(sizeof(nvic) == 3072, "Bad NVIC size");
static nvic *const NVIC = (nvic*)0xe000e100;

/*
 * FPU
 */
struct fpu {
	union fpccr {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbit<S, bool, 31> ASPEN;
		bitfield::armbit<S, bool, 30> LSPEN;
		bitfield::armbit<S, bool, 8> MONRDY;
		bitfield::armbit<S, bool, 6> BFRDY;
		bitfield::armbit<S, bool, 5> MMRDY;
		bitfield::armbit<S, bool, 4> HFRDY;
		bitfield::armbit<S, bool, 3> THREAD;
		bitfield::armbit<S, bool, 1> USER;
		bitfield::armbit<S, bool, 0> LSPACT;
	} FPCCR;
	uint32_t FPCAR;
	uint32_t FPDSCR;
	uint32_t MVFR0;
	uint32_t MVFR1;
	uint32_t MVFR2;
};
static_assert(sizeof(fpu) == 24, "Bad FPU size");
static fpu *const FPU = (fpu*)0xe000ef34;

/*
 * DWT
 */
struct dwt {
	union ctrl {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbits<S, unsigned, 31, 28> NUMCOMP;
		bitfield::armbit<S, bool, 27> NOTRCPKT;
		bitfield::armbit<S, bool, 26> NOEXTTRIG;
		bitfield::armbit<S, bool, 25> NOCYCCNT;
		bitfield::armbit<S, bool, 24> NOPRFCNT;
		bitfield::armbit<S, bool, 22> CYCEVTENA;
		bitfield::armbit<S, bool, 21> FOLDEVTENA;
		bitfield::armbit<S, bool, 20> LSUEVTENA;
		bitfield::armbit<S, bool, 19> SLEEPEVTENA;
		bitfield::armbit<S, bool, 18> EXCEVTENA;
		bitfield::armbit<S, bool, 17> CPIEVTENA;
		bitfield::armbit<S, bool, 16> EXCTRCENA;
		bitfield::armbit<S, bool, 12> PCSAMPLENA;
		bitfield::armbits<S, unsigned, 11, 10> SYNCTAP;
		bitfield::armbit<S, bool, 9> CYCTAP;
		bitfield::armbits<S, unsigned, 8, 5> POSTINIT;
		bitfield::armbits<S, unsigned, 4, 1> POSTPRESET;
		bitfield::armbit<S, bool, 0> CYCCNTENA;
	} CTRL;
	uint32_t CYCCNT;
	uint32_t CPICNT;
	uint32_t EXCCNT;
	uint32_t SLEEPCNT;
	uint32_t LSUCNT;
	uint32_t FOLDCNT;
	uint32_t PCSR;
	uint32_t COMP0;
	uint32_t MASK0;
	uint32_t FUNCTION0;
	uint32_t : 32;
	uint32_t COMP1;
	uint32_t MASK1;
	uint32_t FUNCTION1;
	uint32_t : 32;
	uint32_t COMP2;
	uint32_t MASK2;
	uint32_t FUNCTION2;
	uint32_t : 32;
	uint32_t COMP3;
	uint32_t MASK3;
	uint32_t FUNCTION3;
	uint32_t reserved_0[981];
	uint32_t LAR;
	uint32_t LSR;
};
static_assert(sizeof(dwt) == 0xfb8, "Bad DWT size");
static dwt *const DWT = (dwt*)0xe0001000;

/*
 * MPU
 */
struct mpu {
	union type {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbits<S, unsigned, 23, 16> IREGION;
		bitfield::armbits<S, unsigned, 15, 8> DREGION;
		bitfield::armbit<S, bool, 0> SEPARATE;
	} TYPE;
	union ctrl {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbit<S, bool, 2> PRIVDEFENA;
		bitfield::armbit<S, bool, 1> HFNMIENA;
		bitfield::armbit<S, bool, 0> ENABLE;
	} CTRL;
	uint32_t RNR;
	union rbar {
		using S = uint32_t;
		struct { S r; };
		bitfield::armbits<S, unsigned, 31, 5> ADDR;
		bitfield::armbit<S, bool, 4> VALID;
		bitfield::armbits<S, unsigned, 3, 0> REGION;
	} RBAR;
	union rasr {
		using T = uint32_t;
		struct { T r; };
		enum class xn {
			Execute = 0,
			No_Execute = 1,
		};
		bitfield::armbit<T, xn, 28> XN;
		enum class ap {
			None = 0,
			Kern_RW = 1,
			Kern_RW_User_RO = 2,
			Kern_RW_User_RW = 3,
			Kern_RO = 5,
			Kern_RO_User_RO = 6,
		};
		bitfield::armbits<T, ap, 26, 24> AP;
		bitfield::armbits<T, unsigned, 21, 19> TEX;
		bitfield::armbit<T, bool, 18> S;
		bitfield::armbit<T, bool, 17> C;
		bitfield::armbit<T, bool, 16> B;
		bitfield::armbits<T, unsigned, 15, 8> SRD;
		bitfield::armbits<T, unsigned, 5, 1> SIZE;
		bitfield::armbit<T, bool, 0> ENABLE;

		constexpr
		rasr()
		: r{}
		{}

		constexpr
		rasr(xn xn, ap ap, unsigned tex, bool s, bool c, bool b)
		: r{decltype(XN)(xn).r | decltype(AP)(ap).r |
		    decltype(TEX)(tex).r | decltype(S)(s).r |
		    decltype(C)(c).r | decltype(B)(b).r}
		{}
	} RASR;
	rbar RBAR_A1;
	rasr RASR_A1;
	rbar RBAR_A2;
	rasr RASR_A2;
	rbar RBAR_A3;
	rasr RASR_A3;
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
static_assert(sizeof(mpu) == 0x60, "Bad MPU size");
static mpu *const MPU = (mpu*)0xe000ed90;

/*
 * values for 'flags' argument of mpu_init
 */
#define MPU_ENABLE_DEFAULT_MAP 0x1

/*
 * values for RASR register
 */
constexpr mpu::rasr RASR_KERNEL_RWX_WBWA{
	mpu::rasr::xn::Execute, mpu::rasr::ap::Kern_RW, 0b001, 0, 1, 1};

constexpr mpu::rasr RASR_KERNEL_RW{
	mpu::rasr::xn::No_Execute, mpu::rasr::ap::Kern_RW, 0b001, 0, 0, 0};

constexpr mpu::rasr RASR_USER_R_WBWA{
	mpu::rasr::xn::No_Execute, mpu::rasr::ap::Kern_RW_User_RO, 0b001, 0, 1, 1};

constexpr mpu::rasr RASR_USER_RX_WBWA{
	mpu::rasr::xn::Execute, mpu::rasr::ap::Kern_RW_User_RO, 0b001, 0, 1, 1};

constexpr mpu::rasr RASR_USER_RW_WBWA{
	mpu::rasr::xn::No_Execute, mpu::rasr::ap::Kern_RW_User_RW, 0b001, 0, 1, 1};

constexpr mpu::rasr RASR_USER_RWX_WBWA{
	mpu::rasr::xn::Execute, mpu::rasr::ap::Kern_RW_User_RW, 0b001, 0, 1, 1};

constexpr mpu::rasr RASR_NONE{
	mpu::rasr::xn::No_Execute, mpu::rasr::ap::None, 0, 0, 0, 0};

#endif /* !__ASSEMBLY__ */
