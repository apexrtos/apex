#pragma once

/*
 * arch/backtrace.h - architecture specific backtrace
 */

struct thread;

void arch_backtrace(thread *);
