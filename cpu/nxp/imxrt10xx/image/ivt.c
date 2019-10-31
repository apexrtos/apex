#include "ivt.h"

/*
 * Image Vector Table
 */
extern void *entry[1], *boot_data[1], *dcd[1], *ivt[1];
const struct ivt ivt_d = {
	.header.tag = 0xd1,
	.header.length_be = __builtin_bswap16(sizeof(struct ivt)),
	.header.version = 0x41,
	.entry = entry,
	.dcd = dcd,
	.boot_data = boot_data,
	.self = ivt,
};
