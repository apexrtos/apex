/*
 * load_bootimg.c - load kernel from executable boot image
 */
#include <boot.h>

#include <endian.h>

/*
 * The Apex executable boot image is assembled as follows:
 * 1. Boot loader
 * 2. Zero terminated array of file sizes (32bit big endian)
 * 4. Apex kernel
 * 3. Boot files
 */
int
load_bootimg(void)
{
	extern phys __loader_end[1];
	phys *p, *file_data;
	uint32_t *file_sizes = (uint32_t *)__loader_end;
	size_t files = 0;

	for (uint32_t *it = file_sizes; *it && files < 10; ++it)
		++files;

	if (!files) {
		dbg("No files attached to boot loader\n");
		return -1;
	}
	if (files == 10) {
		dbg("Found more than 10 files. Corrupt boot image?\n");
		return -1;
	}

	file_data = __loader_end + (files + 1) * sizeof(uint32_t);

	dbg("Found %zu boot files:\n", files);
	p = file_data;
	for (size_t i = 0; i < files; ++i) {
		size_t sz = be32toh(file_sizes[i]);
		dbg("  %zu: %p -> %p (%zu bytes)\n", i, p, p + sz, sz);
		p += sz;
	}

	dbg("Loading kernel from file 0\n");
	if (load_elf(file_data))
		return -1;

	if (files > 1) {
		dbg("Passing file 1 to kernel as boot archive\n");
		args.archive_addr = file_data + be32toh(file_sizes[0]);
		args.archive_size = be32toh(file_sizes[1]);
	}

	if (files > 2)
		dbg("Ignoring %zu extra files\n", files - 2);

	return 0;
}
