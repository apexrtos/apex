#include "boot_data.h"

#include <conf/config.h>

/*
 * Boot Data Structure for Execute in Place
 */
const struct boot_data boot_data_ = {
	.start = CONFIG_ROM_BASE_PHYS,
	.length = CONFIG_ROM_SIZE,
	.plugin = 0,
};
