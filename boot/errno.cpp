#include <errno.h>
#include <sys/include/compiler.h>

int *
__errno_location(void)
{
	static int v;
	return &v;
}

weak_alias(__errno_location, ___errno_location);
