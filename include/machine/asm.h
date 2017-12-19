
#if defined(__arm__)
#include "arm/asm.h"
#elif defined (__i386__)
#include "i386/asm.h"
#elif defined (__ppc__)
#include "ppc/asm.h"
#elif defined (__mips__)
#include "mips/asm.h"
#elif defined (__sh4__)
#include "sh4/asm.h"
#else
#error architecture not supported
#endif
