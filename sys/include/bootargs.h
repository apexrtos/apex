#pragma once

/*
 * Arguments passed from bootloader to kernel
 */
struct bootargs {
	unsigned long archive_addr;	/* boot archive address */
	unsigned long archive_size;	/* boot archive size */
	unsigned long machdep0;		/* machine dependent argument 0 */
	unsigned long machdep1;		/* machine dependent argument 1 */
};
