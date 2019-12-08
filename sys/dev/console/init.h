#pragma once

/*
 * Console driver
 */

#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

void console_init(const char *dev, tcflag_t cflag);

#ifdef __cplusplus
} /* extern "C" */
#endif
