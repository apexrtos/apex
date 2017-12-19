
#if defined(__arm__)
#include "arm/stdarg.h"
#elif defined (__i386__)
#include "i386/stdarg.h"
#elif defined (__ppc__)
#include "ppc/stdarg.h"
#elif defined (__mips__)
#include "mips/stdarg.h"
#elif defined (__sh4__)
#include "sh4/stdarg.h"
#else
#error architecture not supported
#endif
