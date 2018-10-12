#include <debug.h>

extern "C" void __cxa_pure_virtual(void)
{
	panic("pure virtual method called\n");
}

extern "C" void __cxa_deleted_virtual(void)
{
	panic("deleted virtual method called\n");
}

