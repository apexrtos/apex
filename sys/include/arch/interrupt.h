#pragma once

/*
 * arch/interrupt.h - architecture specific interrupt management
 */

void interrupt_enable();
void interrupt_disable();
void interrupt_save(int *);
void interrupt_restore(int);
bool interrupt_enabled();
void interrupt_mask(int);
void interrupt_unmask(int, int);
void interrupt_setup(int, int);
void interrupt_init();
int interrupt_to_ist_priority(int);
bool interrupt_from_userspace();
bool interrupt_running();
