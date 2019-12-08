#include <arch.h>

#include <conf/config.h>
#include <conf/drivers.h>
#include <dev/arm/mps2-uart/mps2-uart.h>
#include <interrupt.h>
#include <kernel.h>
#include <page.h>

static const unsigned long UART0 = 0x40004000;

void
machine_init(struct bootargs *args)
{
	const struct meminfo memory[] = {
		/* Main memory */
		{
			.base = (phys *)CONFIG_RAM_BASE_PHYS,
			.size = CONFIG_RAM_SIZE,
			.attr = MA_SPEED_0,
		},
	};
	page_init(memory, ARRAY_SIZE(memory), args);
}

void
machine_driver_init(struct bootargs *bootargs)
{
	/*
	 * Run driver initialisation
	 */
	#include <conf/drivers.c>
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
	arm_mps2_uart_early_init(UART0, CONFIG_EARLY_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	arm_mps2_uart_early_print(UART0, s, len);
}
