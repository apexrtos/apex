#include <arch.h>

#include <cpu.h>
#include <debug.h>
#include <dev/arm/mps2-uart/mps2-uart.h>
#include <dev/bootdisk/bootdisk.h>
#include <interrupt.h>
#include <kernel.h>

static const unsigned long UART0 = 0x40004000;
static const unsigned long UART1 = 0x40005000;
static const unsigned long UART2 = 0x40006000;

void clock_init(void)
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
	bootdisk_init();
	mps2_uart_init(&(struct mps2_uart_desc){.name = "ttyS0", .base = UART0, .ipl = IPL_MIN, .rx_int = 0, .tx_int = 1, .overflow_int = 12});
	mps2_uart_init(&(struct mps2_uart_desc){.name = "ttyS1", .base = UART1, .ipl = IPL_MIN, .rx_int = 2, .tx_int = 3, .overflow_int = 12});
	mps2_uart_init(&(struct mps2_uart_desc){.name = "ttyS2", .base = UART2, .ipl = IPL_MIN, .rx_int = 4, .tx_int = 5, .overflow_int = 12});
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
