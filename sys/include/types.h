#ifndef types_h
#define types_h

#include <conf/config.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define _NSIG 65

typedef struct {
	char dummy;
} phys;

typedef struct k_sigset_t { unsigned long __bits[_NSIG / 8 / sizeof(long)]; } k_sigset_t;

/*
 * Memory Attributes
 */
#define MA_SPEED_0 0x0		    /* slowest memory, e.g. PMEM */
#define MA_SPEED_1 0x1		    /* normal memory, e.g. DRAM */
#define MA_SPEED_2 0x2		    /* faster memory, e.g. SRAM */
#define MA_SPEED_3 0x3		    /* even faster memory, e.g. TCM */
#define MA_SPEED_MASK 0x3
#define MA_DMA 0x4		    /* memory is suitable for DMA */
#define MA_CACHE_COHERENT 0x8	    /* memory is cache coherent */
#define MA_PERSISTENT 0x10	    /* memory is persistent */

#define MA_NORMAL CONFIG_MA_NORMAL_ATTR
#define MA_FAST CONFIG_MA_FAST_ATTR

#endif /* !types_h */
