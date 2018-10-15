#ifndef types_h
#define types_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define _NSIG 65

typedef struct {
	char dummy;
} phys;

typedef struct k_sigset_t { unsigned long __bits[_NSIG / 8 / sizeof(long)]; } k_sigset_t;

enum MEM_TYPE {
	MEM_NORMAL,		    /* normal memory, e.g. DRAM */
	MEM_FAST,		    /* fast memory, e.g. SOC SRAM */
	MEM_DMA,		    /* uncached DMA pool */
	MEM_ALLOC = MEM_FAST + 1,   /* maximum allocatable memory type */
};

#endif /* !types_h */
