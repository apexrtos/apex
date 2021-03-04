#pragma once

/*
 * Console driver
 */

#include <termios.h>

void console_init(const char *dev, tcflag_t cflag);
