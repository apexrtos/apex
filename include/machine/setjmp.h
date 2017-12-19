
#if defined(__arm__)
#include "arm/setjmp.h"
#elif defined (__i386__)
#include "i386/setjmp.h"
#elif defined (__ppc__)
#include "ppc/setjmp.h"
#elif defined (__mips__)
#include "mips/setjmp.h"
#elif defined (__sh4__)
#include "sh4/setjmp.h"
#else
#error architecture not supported
#endif
