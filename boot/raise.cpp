#include <boot.h>

int
raise(int sig)
{
	panic("fatal signal");
}
