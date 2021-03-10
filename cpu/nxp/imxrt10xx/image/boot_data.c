#include "boot_data.h"

#include <assert.h>
#include <conf/config.h>

/*
 * Boot Data Structure to run in RAM
 */
const struct boot_data boot_data_ = {
	.start = CONFIG_LOAD_REGION_BASE_PHYS,
	.length = IMAGE_SIZE,
	.plugin = 0,
};

static_assert(IMAGE_SIZE <= CONFIG_LOAD_REGION_SIZE);
