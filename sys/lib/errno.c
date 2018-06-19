#include <errno.h>
#include <kernel.h>
#include <thread.h>

int *
__errno_location(void)
{
	return &thread_cur()->errno_storage;
}
