
#if defined(__arm__)
#include "arm/limits.h"
#elif defined (__i386__)
#include "i386/limits.h"
#elif defined (__ppc__)
#include "ppc/limits.h"
#elif defined (__mips__)
#include "mips/limits.h"
#elif defined (__sh4__)
#include "sh4/limits.h"
#else
#error architecture not supported
#endif
