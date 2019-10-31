#ifndef imxrt10xx_boot_data_h
#define imxrt10xx_boot_data_h

#include <stdint.h>

/*
 * Boot Data Structure
 */
struct boot_data {
	uint32_t start;
	uint32_t length;
	uint32_t plugin;
};

#endif
