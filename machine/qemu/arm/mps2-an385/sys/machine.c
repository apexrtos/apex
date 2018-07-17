#include <arch.h>

#include <conf/config.h>
#include <conf/drivers.h>
#include <cpu.h>
#include <debug.h>
#include <dev/arm/mps2-uart/mps2-uart.h>
#include <interrupt.h>
#include <kernel.h>

static const unsigned long UART0 = 0x40004000;

void
clock_init(void)
{
	/* set systick timer to interrupt us at CONFIG_HZ */
	SYST->RVR = SYST->CVR = 25000000 / CONFIG_HZ;

	/* enable timer & interrupts */
	SYST->CSR = (union syst_csr){
		.ENABLE = 1,
		.TICKINT = 1,
		.CLKSOURCE = 1,	    /* SYSCLK, 25MHz */
	};
}

void
machine_memory_init(void)
{
	/* nothing to do for now */
}

void
machine_init(void)
{
	/* nothing to do for now */
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
	#warning implement
}

void
machine_poweroff(void)
{
	#warning implement
}

void
machine_suspend(void)
{
	#warning implement
}

noreturn void
machine_panic(void)
{
	while (1);
}

void
early_console_init(void)
{
	/* QEMU doesn't care about baud rate */
	mps2_uart_early_init(UART0, CONFIG_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	mps2_uart_early_print(UART0, s, len);
}
