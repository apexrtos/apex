
#if defined(__arm__)
#include "arm/elf.h"
#elif defined (__i386__)
#include "i386/elf.h"
#elif defined (__ppc__)
#include "ppc/elf.h"
#elif defined (__mips__)
#include "mips/elf.h"
#elif defined (__sh4__)
#include "sh4/elf.h"
#else
#error architecture not supported
#endif
