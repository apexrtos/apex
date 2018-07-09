#include <cpu/nxp/imxrt10xx/boot_data.h>

#include <conf/config.h>

/*
 * Boot Data Structure
 */
const struct boot_data boot_data = {
	.start = CONFIG_ROM_BASE_PHYS,
	.length = CONFIG_ROM_SIZE,
	.plugin = 0,
};
