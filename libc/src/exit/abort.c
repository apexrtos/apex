#include <stdlib.h>
#include <signal.h>
#include "syscall.h"

#include <debug.h>

_Noreturn void abort(void)
{
	panic("abort");
}
