#include <boot.h>

extern "C" int
raise(int sig)
{
	panic("fatal signal");
}
