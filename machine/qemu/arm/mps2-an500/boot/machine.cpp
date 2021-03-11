/*
 * See AN500 - ARM Cortex-M7 SMM on V2M-MPS2+, aka DAI0500B
 * See Cortex-M System Design Kit Technical Reference, aka DDI0479B
 */

#include <machine.h>

#include <boot.h>
#include <sys/dev/arm/mps2-uart/early.h>

#if defined(CONFIG_BOOT_CONSOLE)
constexpr auto UART0 = 0x40004000;
#endif

/*
 * Setup machine state
 */
void machine_setup()
{
#if defined(CONFIG_BOOT_CONSOLE)
	/* QEMU doesn't care about baud rate */
	arm::mps2_uart_early_init(UART0, CONFIG_EARLY_CONSOLE_CFLAG);
#endif
}

/*
 * Print a string
 */
void machine_print(const char *s, size_t len)
{
#if defined(CONFIG_BOOT_CONSOLE)
	arm::mps2_uart_early_print(UART0, s, len);
#endif
}

/*
 * Load kernel image
 */
int machine_load_image()
{
	return load_bootimg();
}

/*
 * Panic handler
 */
void machine_panic()
{
	/* Workaround for ancient clang bug. Looks like this will be fixed
	 * in clang 12,  https://reviews.llvm.org/D85393 */
	while (1) asm("");
}

/*
 * Initialise clocks.
 */
extern "C" void machine_clock_init()
{
	/* QEMU doesn't require clock initialisation */
}

/*
 * Initialise stack
 */
extern "C" void machine_early_memory_init()
{
	/* QEMU doesn't require stack initialisation */
}

/*
 * Initialise memory
 */
extern "C" void machine_memory_init()
{
	/* QEMU doesn't require memory initialisation */
}
