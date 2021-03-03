#pragma once

#include <stdint.h>

/*
 * Device Configuration Data
 */
struct dcd_header {
	uint8_t tag;
	uint16_t length_be;	    /* big endian */
	uint8_t version;
} __attribute__((packed));

struct dcd_command {
	uint8_t tag;
	uint16_t length_be;	    /* big endian */
	uint8_t parameter;
} __attribute__((packed));

#define DCD_HEADER_TAG 0xd2
#define DCD_HEADER_VERSION 0x41

#define DCD_WRITE_TAG 0xcc
#define DCD_WRITE_PARAM_WRITE32 0x4

#define DCD_CHECK_DATA_TAG 0xcf
#define DCD_CHECK_DATA_PARAM_ANY_SET32 0x1c
