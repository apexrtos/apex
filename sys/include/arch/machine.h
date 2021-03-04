#pragma once

/*
 * arch/machine.h - architecture specific machine management
 */

struct bootargs;

void machine_init(bootargs *);
void machine_driver_init(bootargs *);
void machine_idle();
[[noreturn]] void machine_reset();
void machine_poweroff();
void machine_suspend();
[[noreturn]] void machine_panic();
