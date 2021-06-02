/*
 * Generic QEMU paravirtualised e500 platform.
 */

#include <machine.h>

#include "ns16550/ns16550.h"
#include <boot.h>

/*
 * NS16550 compatible UART at physical address 0xfe0004500.
 * If the boot console is enabled machine_init maps this to 0xfe004500.
 */
constexpr auto UART = 0xfe004500;

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
	serial::ns16550::early_init(UART, 0, CONFIG_EARLY_CONSOLE_CFLAG);
}

/*
 * Print a string
 */
void
boot_console_print(const char *s, size_t len)
{
	serial::ns16550::early_print(UART, s, len);
}

