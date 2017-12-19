
#if defined(__arm__)
#include "arm/endian.h"
#elif defined (__i386__)
#include "i386/endian.h"
#elif defined (__ppc__)
#include "ppc/endian.h"
#elif defined (__mips__)
#include "mips/endian.h"
#elif defined (__sh4__)
#include "sh4/endian.h"
#else
#error architecture not supported
#endif
