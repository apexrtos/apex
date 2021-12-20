#include <arch/early_console.h>
#include <arch/machine.h>

#include <arch/mmio.h>
#include <arch/mmu.h>
#include <compiler.h>
#include <conf/config.h>
#include <conf/drivers.h>
#include <cpu.h>
#include <debug.h>
#include <dev/intc/sifive/clint/clint.h>
#include <dev/intc/sifive/plic/plic.h>
#include <dev/serial/ns16550/ns16550.h>
#include <intrinsics.h>
#include <locore.h>
#include <page.h>
#include <sch.h>
#include <sections.h>
#include <thread.h>
#include <types.h>

constexpr auto UART = 0x10000000_phys;

void
machine_init(bootargs *args)
{
#ifdef CONFIG_MPU
	mpu_init(nullptr, 0, 0);
#endif

	const meminfo memory[] = {
		/* Main memory */
		{
			.base = phys{CONFIG_RAM_BASE_PHYS},
			.size = CONFIG_RAM_SIZE,
			.attr = MA_SPEED_0,
			.priority = 0,
		},
	};
	page_init(memory, ARRAY_SIZE(memory), args);

	/* scratch register is 0 in kernel mode */
#ifdef CONFIG_S_MODE
	csrw(sscratch{});
#else
	csrw(mscratch{});
#endif
	/* initialise kernel thread pointer */
	asm("mv tp, %0" : : "r"(thread_cur()));
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
	asm("wfi");
}

[[noreturn]] void
machine_reset()
{
	write32(reinterpret_cast<int *>(0x100000), 0x7777);

	/* Workaround clang bug. */
	while (1) asm("");
}

void
machine_poweroff()
{
	write32(reinterpret_cast<int *>(0x100000), 0x5555);

	/* Workaround clang bug. */
	while (1) asm("");
}

void
machine_suspend()
{
	info("Suspend is not supported on this platform.\n");
}

[[noreturn]] void
machine_panic()
{
	/* Workaround clang bug. */
	while (1) asm("");
}

void
early_console_init()
{
	/* QEMU doesn't care about baud rate */
	serial::ns16550::early_init(phys_to_virt(UART), 0, CONFIG_EARLY_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	serial::ns16550::early_print(phys_to_virt(UART), s, len);
}

__fast_text
void
machine_irq()
{
	intc_sifive_plic_irq();
}

__fast_text
void
machine_timer()
{
	intc_sifive_clint_timer_irq();
}
