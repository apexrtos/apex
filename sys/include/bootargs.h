#ifndef bootargs_h
#define bootargs_h

#include <sys/include/types.h>

/*
 * Arguments passed from bootloader to kernel
 */
struct bootargs {
	phys *archive_addr;	/* boot archive address */
	long archive_size;	/* boot archive size */
	long machdep0;		/* machine dependent argument 0 */
	long machdep1;		/* machine dependent argument 1 */
};

#endif
