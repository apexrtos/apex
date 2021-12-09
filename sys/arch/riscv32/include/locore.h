#pragma once

/*
 * locore.h -  low level platform support
 */

#include <cassert>
#include <conf/config.h>
#include <cstdint>

struct trap_frame;

extern "C" long arch_syscall(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
			     uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7);
extern "C" void handle_trap(uint32_t cause, trap_frame *);
extern "C" void mpu_user_thread_switch();
extern "C" void return_to_user();
extern "C" void thread_entry();
void machine_irq();
void machine_timer();

/*
 * context_frame
 */
struct context_frame {
	uint32_t ra;
	uint32_t s[12];
	uint32_t pad[3];
};
static_assert(sizeof(context_frame) % 16 == 0);

/*
 * trap_frame
 */
struct trap_frame {
	uint32_t ra, gp;
	uint32_t a[8];
	uint32_t s[12];
	uint32_t t[7];
	uint32_t xepc, xtval, xstatus, tp, sp;
	uint32_t pad[2];
};
static_assert(sizeof(trap_frame) % 16 == 0);
