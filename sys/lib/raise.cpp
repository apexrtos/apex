#include <debug.h>

int
raise(int sig)
{
	panic("fatal signal");
}
