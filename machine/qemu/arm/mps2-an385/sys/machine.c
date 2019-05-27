#include <arch.h>

#include <bootinfo.h>
#include <conf/config.h>
#include <conf/drivers.h>
#include <debug.h>
#include <dev/arm/mps2-uart/mps2-uart.h>
#include <interrupt.h>
#include <kernel.h>

static const unsigned long UART0 = 0x40004000;

void
machine_init(struct bootargs *args)
{
	unsigned i = bootinfo.nr_rams;

	/* Main memory */
	bootinfo.ram[i].base = (void*)CONFIG_RAM_BASE_PHYS;
	bootinfo.ram[i].size = CONFIG_RAM_SIZE;
	bootinfo.ram[i].type = MT_NORMAL;
	++i;

	bootinfo.nr_rams = i;
}

void
machine_driver_init(struct bootargs *bootargs)
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
	arm_mps2_uart_early_init(UART0, CONFIG_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	arm_mps2_uart_early_print(UART0, s, len);
}
