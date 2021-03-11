#include <arch/early_console.h>
#include <arch/machine.h>

#include <compiler.h>
#include <conf/config.h>
#include <conf/drivers.h>
#include <dev/arm/mps2-uart/early.h>
#include <interrupt.h>
#include <page.h>
#include <types.h>

constexpr auto UART0 = 0x40004000;

void
machine_init(bootargs *args)
{
	const meminfo memory[] = {
		/* Main memory */
		{
			.base = (phys *)CONFIG_RAM_BASE_PHYS,
			.size = CONFIG_RAM_SIZE,
			.attr = MA_SPEED_0,
			.priority = 0,
		},
	};
	page_init(memory, ARRAY_SIZE(memory), args);
}

void
machine_driver_init(bootargs *bootargs)
{
	/*
	 * Run driver initialisation
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
	#warning implement

	/* Workaround for ancient clang bug. Looks like this will be fixed
	 * in clang 12,  https://reviews.llvm.org/D85393 */
	while (1) asm("");
}

void
machine_poweroff()
{
	#warning implement
}

void
machine_suspend()
{
	#warning implement
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
	/* QEMU doesn't care about baud rate */
	arm::mps2_uart_early_init(UART0, CONFIG_EARLY_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	arm::mps2_uart_early_print(UART0, s, len);
}
