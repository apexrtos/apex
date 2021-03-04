#pragma once

/*
 * arch/early_console.h - architecture specific early console
 */

#include <cstddef>

void early_console_init();
void early_console_print(const char *, size_t);
