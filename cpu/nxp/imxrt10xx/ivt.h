#ifndef imxrt10xx_ivt_h
#define imxrt10xx_ivt_h

#include <assert.h>
#include <stdint.h>

/*
 * Image Vector Table
 */
struct ivt {
	struct __attribute__((packed)) {
		uint8_t tag;
		uint16_t length_be;	    /* big endian */
		uint8_t version;
	} header;
	const void *entry;
	uint32_t : 32;
	const void *dcd;
	const void *boot_data;
	const void *self;
	const void *csf;
	uint32_t reserved : 32;
};
static_assert(sizeof(struct ivt) == 32, "");

#endif
