#pragma once

/*
 * arch/stack.h - architecture specific stack management
 */

void *arch_ustack_align(void *);
void *arch_kstack_align(void *);
