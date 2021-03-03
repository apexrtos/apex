#pragma once

#include <stdint.h>

/*
 * Boot Data Structure
 */
struct boot_data {
	uint32_t start;
	uint32_t length;
	uint32_t plugin;
};
