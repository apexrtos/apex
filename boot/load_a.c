/*
 * load_a.c - load kernel from archive
 */
#include <boot.h>

#include <ar.h>
#include <endian.h>
#include <stdlib.h>
#include <string.h>

/*
 * Load kernel from archive. Pass archive location to kernel.
 */
int
load_a(void)
{
	extern char __loader_end;
	const char *ar = &__loader_end;

	const long ar_size = be32toh(*(long*)ar);
	ar += sizeof(long);

	/* Hopefully we have an archive */
	if (strncmp(ar, ARMAG, SARMAG)) {
		dbg("Bad boot archive at %p (%lu)\n", ar, ar_size);
		return -1;
	}
	dbg("Found boot archive %lu bytes\n", ar_size);

	/*
	 * Pass location and size of boot archive to kernel
	 *
	 * ELF loader also checks that this does not overlap the kernel image
	 */
	int i = bootinfo->nr_rams;
	bootinfo->ram[i].base = (void *)ar;
	bootinfo->ram[i].size = ar_size;
	bootinfo->ram[i].type = MT_BOOTDISK;
	bootinfo->nr_rams++;

	ar += SARMAG;

	/* Try to find kernel */
	while (1) {
		struct ar_hdr *hdr = (struct ar_hdr *)ar;
		ar += sizeof(struct ar_hdr);

		/* Check archive header */
		if (strncmp(hdr->ar_fmag, ARFMAG, sizeof(hdr->ar_fmag))) {
			dbg("Archive header check failed\n");
			return -1;
		}

		/* Match kernel filename */
		if (strncmp(hdr->ar_name, "apex/", 5)) {
			ar += atol(hdr->ar_size);
			ar += (uintptr_t)ar % 2;
			continue;
		}

		/* Try to load elf file */
		if (load_elf(ar))
			return -1;

		return 0;
	}

	dbg("Archive missing kernel\n");
	return -1;
}
