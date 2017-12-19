
#if defined(__arm__)
#include "arm/lock.h"
#elif defined (__i386__)
#include "i386/lock.h"
#elif defined (__ppc__)
#include "ppc/lock.h"
#elif defined (__mips__)
#include "mips/lock.h"
#elif defined (__sh4__)
#include "sh4/lock.h"
#else
#error architecture not supported
#endif
