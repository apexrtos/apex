#include <errno.h>
#include <thread.h>
#include <compiler.h>

int *
__errno_location()
{
	return &thread_cur()->errno_storage;
}

weak_alias(__errno_location, ___errno_location);
