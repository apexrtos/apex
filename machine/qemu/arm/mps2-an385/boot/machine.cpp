/*
 * See AN385 - ARM Cortex-M3 SMM on V2M-MPS2, aka DAI0385D
 * See Cortex-M System Design Kit Technical Reference, aka DDI0479B
 */

#include <machine.h>

#include "mps2-uart/early.h"
#include <boot.h>

constexpr auto UART0 = 0x40004000;

/*
 * Setup machine state
 */
void
machine_setup()
{
	/* QEMU doesn't require setup */
}

/*
 * Load kernel image
 */
int
machine_load_image()
{
	return load_bootimg();
}

/*
 * Panic handler
 */
[[noreturn]] void
machine_panic()
{
	/* Workaround for ancient clang bug. Looks like this will be fixed
	 * in clang 12,  https://reviews.llvm.org/D85393 */
	while (1) asm("");
}

/*
 * Configure boot console
 */
void
boot_console_init()
{
	arm::mps2_uart_early_init(UART0, CONFIG_EARLY_CONSOLE_CFLAG);
}

/*
 * Print a string
 */
void
boot_console_print(const char *s, size_t len)
{
	arm::mps2_uart_early_print(UART0, s, len);
}

/*
 * Initialise clocks.
 */
extern "C" void
arm_v7m_clock_init()
{
	/* QEMU doesn't require clock initialisation */
}

/*
 * Initialise stack
 */
extern "C" void
arm_v7m_early_memory_init()
{
	/* QEMU doesn't require stack initialisation */
}

/*
 * Initialise memory
 */
extern "C" void
arm_v7m_memory_init()
{
	/* QEMU doesn't require memory initialisation */
}
