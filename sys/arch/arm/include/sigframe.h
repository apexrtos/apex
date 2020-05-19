#pragma once

#include <stdint.h>

/*
 * Structures stored in the uc_regspace area of the signal frame
 */

/*
 * VFP
 */
#define VFP_SIGFRAME_MAGIC 0x56465001
struct vfp_sigframe {
	uint32_t magic;
	uint32_t size;
	uint32_t regs[64];
	uint32_t fpscr;
	uint32_t fpexc;
	uint32_t fpinst;
	uint32_t fpinst2;
};

