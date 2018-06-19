/*
 * bootinfo.c - storage for bootinfo
 */

#include <bootinfo.h>
#include <string.h>

struct bootinfo bootinfo;

void
copy_bootinfo(struct bootinfo *orig)
{
	memcpy(&bootinfo, orig, sizeof(bootinfo));
}
