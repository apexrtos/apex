#include <errno.h>

int *
__errno_location(void)
{
	static int v;
	return &v;
}
