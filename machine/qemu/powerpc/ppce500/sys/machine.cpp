#include <arch/early_console.h>
#include <arch/machine.h>

#include <arch/mmu.h>
#include <booke/locore.h>
#include <compiler.h>
#include <conf/config.h>
#include <conf/drivers.h>
#include <cpu.h>
#include <debug.h>
#include <dev/intc/openpic/openpic.h>
#include <dev/serial/ns16550/ns16550.h>
#include <page.h>
#include <sections.h>
#include <sys/mman.h>
#include <types.h>

/*
 * NS16550 compatible UART at physical address 0xfe0004500.
 */
constexpr auto UART = 0xfe004500;
constexpr auto mas2_device = decltype(MAS2::I)(true).r |
			     decltype(MAS2::G)(true).r;

/*
 * IRQ stacks for boot CPU
 */
alignas(16) static char base_stack[CONFIG_IRQSTACK_SIZE];
alignas(16) static char critical_stack[CONFIG_IRQSTACK_SIZE];
#if defined(POWER_MACHINE_CHECK_EXTENSION)
alignas(16) static char machine_check_stack[CONFIG_IRQSTACK_SIZE];
#endif
#if defined(POWER_CAT_E_ED)
alignas(16) static char debug_stack[CONFIG_IRQSTACK_SIZE];
#endif

/*
 * cpu_data for boot CPU
 */
static const cpu_data cpu {
	.base_irq_stack = base_stack + CONFIG_IRQSTACK_SIZE,
	.critical_irq_stack = critical_stack + CONFIG_IRQSTACK_SIZE,
#if defined(POWER_MACHINE_CHECK_EXTENSION)
	.machine_check_irq_stack = machine_check_stack + CONFIG_IRQSTACK_SIZE,
#endif
#if defined(POWER_CAT_E_ED)
	.debug_irq_stack = debug_stack + CONFIG_IRQSTACK_SIZE,
#endif
};

void
machine_init(bootargs *args)
{
	const mmumap maps[] = {
		/* Main Memory */
		{
			.paddr = phys{CONFIG_RAM_BASE_PHYS},
			.vaddr = (void *)CONFIG_RAM_BASE_VIRT,
			.size = 0x40000000, /* 1GiB */
			.prot = PROT_READ | PROT_WRITE | PROT_EXEC,
			.flags = 0,
		},
		/* CCSR - Configuration, Control and Status Registers */
		{
			.paddr = 0xfe0000000_phys,
			.vaddr = (void *)0xfe000000,
			.size = 0x100000,
			.prot = PROT_READ | PROT_WRITE,
			.flags = mas2_device,
		},
	};
	mmu_init(maps);

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

	/* Initialise CPU state */
	mtspr<CPU_DATA>({&cpu});
	mtspr<IRQ_NESTING>({0});
	mtmsr([] {
		/* debug, critical and machine check exceptions can be handled
		   as soon as CPU_DATA is initialised */
		auto v = mfmsr();
		v.DE = true;
		v.CE = true;
		v.ME = true;
		return v;
	}());
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
	/* map NS16550 UART at 0xfe0004500 to 0xfe004500 */
#warning we do this twice - BOOT_CONSOLE and panic
#warning on panic it's probably already mapped via mmu_init?
#warning also mapped by machine_init CCSR mapping?
	mmu_early_map(0xfe0004000_phys, (void *)UART, 0x1000, mas2_device);
	serial::ns16550::early_init(UART, 0, CONFIG_EARLY_CONSOLE_CFLAG);
}

void
early_console_print(const char *s, size_t len)
{
	serial::ns16550::early_print(UART, s, len);
}

extern "C" __fast_text void
exc_External_Input()
{
	intc_openpic_irq();
}
