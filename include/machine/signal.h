
#if defined(__arm__)
#include "arm/signal.h"
#elif defined (__i386__)
#include "i386/signal.h"
#elif defined (__ppc__)
#include "ppc/signal.h"
#elif defined (__mips__)
#include "mips/signal.h"
#elif defined (__sh4__)
#include "sh4/signal.h"
#else
#error architecture not supported
#endif
