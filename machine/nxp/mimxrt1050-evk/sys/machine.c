#include <arch.h>

#include <conf/config.h>
#include <conf/drivers.h>
#include <cpu.h>
#include <cpu/nxp/imxrt10xx/iomuxc.h>
#include <cpu/nxp/imxrt10xx/src.h>
#include <debug.h>
#include <dev/nxp/imxrt-lpuart/imxrt-lpuart.h>
#include <interrupt.h>
#include <kernel.h>
#include <timer.h>

static const unsigned long LPUART1 = 0x40184000;

void
clock_init(void)
{
	/*
	 * REVISIT: NXP documentation says "24 MHz XTALOSC can be the external
	 * clock source of SYSTICK" however, experimentation shows that it is
	 * probably running at 100KHz.
	 */

	/* set systick timer to interrupt us at CONFIG_HZ */
	SYST->RVR = SYST->CVR = 100000 / CONFIG_HZ;

	/* enable timer & interrupts */
	SYST->CSR = (union syst_csr){
		.ENABLE = 1,
		.TICKINT = 1,
		.CLKSOURCE = 0,	    /* EXTERNAL, 24MHz */
	};
}

void
machine_memory_init(void)
{
	/* IMXRT10xx places external SDRAM in a default write-through memory
	   region. Override this as write-back. */
	struct mmumap mappings[] = {
		{
			.paddr = (phys *)0x80000000,
			.size = CONFIG_DRAM_SIZE,
			.flags = RASR_KERNEL_RWX_WBWA,
		},
	};
	mpu_init(mappings, ARRAY_SIZE(mappings), MPU_ENABLE_DEFAULT_MAP);
}

void
machine_init(void)
{
	#include <conf/machcfg.c>
	#include <conf/pincfg.c>
}

void
machine_driver_init(void)
{
	#include <conf/drivers.c>
}

void
machine_ready(void)
{
	/* nothing to do for now */
}

void
machine_idle(void)
{
	/* nothing to do for now */
}

void
machine_reset(void)
{
	/* wait for console messages to finish printing */
	timer_delay(0.25 * 1e9);

	/* some ISRs are in ITCM which is about to disappear */
	interrupt_disable();

	/* reset flexram configuration -- this is necessary as the IMXRT1050
	 * boot ROM expects to use OCRAM as stack and is too stupid to make
	 * sure that it will actually work */
	IOMUXC_GPR->GPR16.FLEXRAM_BANK_CFG_SEL = 0;
	asm volatile("dsb");

	/* assert reset */
	SRC->SCR.CORE0_RST = 1;
	asm volatile("dsb");

	while (1);
}

void
machine_poweroff(void)
{
	info("machine_poweroff not supported\n");
}

void
machine_suspend(void)
{
	info("machine_suspend not supported\n");
}

noreturn void
machine_panic(void)
{
	while (1);
}

void
early_console_init(void)
{
	imxrt_lpuart_early_init(LPUART1, 24000000, CONFIG_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	imxrt_lpuart_early_print(LPUART1, s, len);
}
