
#if defined(__arm__)
#include "arm/systrap.h"
#elif defined (__i386__)
#include "i386/systrap.h"
#elif defined (__ppc__)
#include "ppc/systrap.h"
#elif defined (__mips__)
#include "mips/systrap.h"
#elif defined (__sh4__)
#include "sh4/systrap.h"
#else
#error architecture not supported
#endif
