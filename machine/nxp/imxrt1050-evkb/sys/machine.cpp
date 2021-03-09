#include <arch/early_console.h>
#include <arch/machine.h>

#include <arch/barrier.h>
#include <arch/interrupt.h>
#include <arch/mmio.h>
#include <arch/mmu.h>
#include <compiler.h>
#include <conf/config.h>
#include <conf/drivers.h>
#include <cpu.h>
#include <cpu/nxp/imxrt10xx/ccm.h>
#include <cpu/nxp/imxrt10xx/iomuxc.h>
#include <cpu/nxp/imxrt10xx/pads.h>
#include <cpu/nxp/imxrt10xx/pmu.h>
#include <cpu/nxp/imxrt10xx/src.h>
#include <debug.h>
#include <dev/fsl/lpuart/lpuart.h>
#include <interrupt.h>
#include <page.h>
#include <timer.h>

constexpr auto LPUART1 = 0x40184000;

void
machine_init(bootargs *args)
{
	const mmumap mappings[] = {
		/* IMXRT10xx places external SDRAM in a default write-through
		 * memory region. Override this as write-back. */
		{
			.paddr = (phys *)CONFIG_DRAM_BASE_PHYS,
			.size = CONFIG_DRAM_SIZE,
			.flags = RASR_KERNEL_RWX_WBWA,
		},
#if (CONFIG_KERNEL_NULL_GUARD_SIZE > 0)
		/* REVISIT: Use debug hardware instead of wasting MPU entry? */
		{
			.paddr = (phys *)0,
			.size = CONFIG_KERNEL_NULL_GUARD_SIZE,
			.flags = RASR_NONE,
		},
#endif
#if (CONFIG_DMA_SIZE > 0)
		/* IMXRT10xx places internal SRAM in default write-back
		 * memory region. Override DMA pool as uncached. */
		{
			.paddr = (phys *)CONFIG_DMA_BASE_PHYS,
			.size = CONFIG_DMA_SIZE,
			.flags = RASR_KERNEL_RW,
		}
#endif
	};
	mpu_init(mappings, ARRAY_SIZE(mappings), MPU_ENABLE_DEFAULT_MAP);

	const meminfo memory[] = {
		/* DRAM */
		{
			.base = (phys*)CONFIG_DRAM_BASE_PHYS,
			.size = CONFIG_DRAM_SIZE,
			.attr = MA_SPEED_0 | MA_DMA,
			.priority = 0,
		},
		/* DTCM */
		{
			.base = (phys*)CONFIG_DTCM_BASE_PHYS,
			.size = CONFIG_DTCM_SIZE,
			.attr = MA_SPEED_2 | MA_SECURE,
			.priority = 2,
		},
		/* DMA */
		{
			.base = (phys*)CONFIG_DMA_BASE_PHYS,
			.size = CONFIG_DMA_SIZE,
			.attr = MA_SPEED_1 | MA_DMA | MA_CACHE_COHERENT | MA_SECURE,
			.priority = 3,
		},
#if defined(CONFIG_SRAM_SIZE)
		/* SRAM */
		{
			.base = (phys*)CONFIG_SRAM_BASE_PHYS,
			.size = CONFIG_SRAM_SIZE,
			.attr = MA_SPEED_1 | MA_DMA | MA_SECURE,
			.priority = 1,
		},
#endif
	};
	page_init(memory, ARRAY_SIZE(memory), args);

	/*
	 * Run machine initialisation.
	 */
	#include <conf/machcfg.c>

	/*
	 * Run pin initialisation.
	 */
	#include <conf/pincfg.c>
}

void
machine_driver_init(bootargs *bootargs)
{
	/*
	 * Run driver initialisation.
	 */
	#include <conf/drivers.c>
}

void
machine_idle()
{
	/* nothing to do for now */
}

[[noreturn]] void
machine_reset()
{
	/* wait for console messages to finish printing */
	timer_delay(0.25 * 1e9);

	/* some ISRs are in ITCM which is about to disappear */
	interrupt_disable();

	/* reset flexram configuration -- this is necessary as the IMXRT1050
	 * boot ROM expects to use OCRAM as stack and is too stupid to make
	 * sure that it will actually work */
	iomuxc_gpr::gpr16 gpr16 = read32(&IOMUXC_GPR->GPR16);
	gpr16.FLEXRAM_BANK_CFG_SEL = 0;
	write32(&IOMUXC_GPR->GPR16, gpr16.r);

	/* wait for FLEXRAM_BANK_CFG_SEL write before asserting reset */
	memory_barrier();

	/* assert reset */
	write32(&SCB->AIRCR, (scb::aircr){{
		.SYSRESETREQ = 1,
		.VECTKEY = 0x05fa,
	}}.r);
	memory_barrier();

	/* Workaround for ancient clang bug. Looks like this will be fixed
	 * in clang 12,  https://reviews.llvm.org/D85393 */
	while (1) asm("");
}

void
machine_poweroff()
{
	info("machine_poweroff not supported\n");
}

void
machine_suspend()
{
	info("machine_suspend not supported\n");
}

[[noreturn]] void
machine_panic()
{
	/* Workaround for ancient clang bug. Looks like this will be fixed
	 * in clang 12,  https://reviews.llvm.org/D85393 */
	while (1) asm("");
}

void
early_console_init()
{
	/* set GPIO_AD_B0_12 as LPUART1_TX */
	write32(&IOMUXC->SW_MUX_CTL_PAD_GPIO_AD_B0_12, (iomuxc::sw_mux_ctl){{
		.MUX_MODE = 2,
		.SION = iomuxc::sw_mux_ctl::sion::Software_Input_On_Disabled,
	}}.r);
	write32(&IOMUXC->SW_PAD_CTL_PAD_GPIO_AD_B0_12, (iomuxc::sw_pad_ctl){{
		.SRE = iomuxc::sw_pad_ctl::sre::Slow,
		.DSE = iomuxc::sw_pad_ctl::dse::R0_6,
		.SPEED = iomuxc::sw_pad_ctl::speed::MHz_100,
		.ODE = iomuxc::sw_pad_ctl::ode::Open_Drain_Disabled,
		.PKE = iomuxc::sw_pad_ctl::pke::Pull_Keeper_Enabled,
		.PUE = iomuxc::sw_pad_ctl::pue::Keeper,
		.PUS = iomuxc::sw_pad_ctl::pus::Pull_Down_100K,
		.HYS = iomuxc::sw_pad_ctl::hys::Hysteresis_Disabled,
	}}.r);

	fsl_lpuart_early_init(LPUART1, 24000000, CONFIG_EARLY_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	fsl_lpuart_early_print(LPUART1, s, len);
}
