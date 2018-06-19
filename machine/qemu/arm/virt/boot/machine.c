/*
 * To extract the device tree for QEMUs ARM virtual machine:
 * qemu-system-arm -machine virt,dumpdtb=virt.dtb
 * dtc -I dtb -O dts virt.dtb
 *
 * PL011 documentation:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ddi0183f/DDI0183.pdf
 *
 * WARNING: This is the bare minimum required to get the PL011 running under
 * QEMU. It WILL NOT be functional on real hardware.
 */

#include <machine.h>

#include <boot.h>

struct pl011 {
	union pl011_dr {
		uint32_t r;
		struct {
			uint32_t DATA : 8;
			uint32_t FE : 1;
			uint32_t PE : 1;
			uint32_t BE : 1;
			uint32_t OE : 1;
			uint32_t : 20;
		};
	} DR;
	union {
		uint32_t RSR;
		uint32_t ECR;
	};
	uint32_t __res0[4];
	const union pl011_fr {
		uint32_t r;
		struct {
			uint32_t CTS : 1;
			uint32_t DSR : 1;
			uint32_t DCD : 1;
			uint32_t BUSY : 1;
			uint32_t RXFE : 1;
			uint32_t TXFF : 1;
			uint32_t RXFF : 1;
			uint32_t TXFE : 1;
			uint32_t RI : 1;
			uint32_t : 23;
		};
	} FR;
	uint32_t __res1;
	uint32_t ILPR;
	uint32_t IBRD;
	uint32_t FBRD;
	uint32_t LCR_H;
	uint32_t CR;
	uint32_t IFLS;
	uint32_t IMSC;
	uint32_t RIS;
	uint32_t MIS;
	uint32_t ICR;
	uint32_t DMACR;
};

static volatile struct pl011 *const UART = (struct pl011*)0x9000000;

/*
 * Setup machine state
 */
void
machine_setup(void)
{
	bootinfo->ram[0].base = CONFIG_RAM_BASE_PHYS;
	bootinfo->ram[0].size = CONFIG_RAM_SIZE;
	bootinfo->ram[0].type = MT_USABLE;
	bootinfo->ram[1].base = (void*)CONFIG_BOOTSTACK_BASE_PHYS;
	bootinfo->ram[1].size = CONFIG_BOOTSTACK_SIZE;
	bootinfo->ram[1].type = MT_RESERVED;
	bootinfo->nr_rams = 2;
}

/*
 * Print one chracter
 */
void
machine_putc(int c)
{
#if defined(CONFIG_DIAG_SERIAL)
	UART->DR.r = c;
#endif
}

/*
 * Load kernel image
 */
int machine_load_image(void)
{
	return load_a();
}

/*
 * Panic handler
 */
void
machine_panic(void)
{
	while (1);
}

