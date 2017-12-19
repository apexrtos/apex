
#if defined(__gba__)
#include "../gba/platform.h"
#elif defined(__integrator__)
#include "../integrator/platform.h"
#else
#error platform not supported
#endif
