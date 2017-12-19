
#if defined(__arm__)
#include "arm/types.h"
#elif defined (__i386__)
#include "i386/types.h"
#elif defined (__ppc__)
#include "ppc/types.h"
#elif defined (__mips__)
#include "mips/types.h"
#elif defined (__sh4__)
#include "sh4/types.h"
#else
#error architecture not supported
#endif
