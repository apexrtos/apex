#include <cpu.h>
#include <cstddef>
#include <locore.h>
#include <thread.h>

#define text(t) __asm__("\n#__OUT__" t)
#define define(t, v) __asm__("\n#__OUT__#define " t " %0" :: "n" (v))

void asm_def()
{
	text("#pragma once");
	text();
	text("/*");
	text(" * asm_def.h - Automatically generated file. Do not edit.");
	text(" */");
	text();
	text("/* context_frame */");
	define("CONTEXT_FRAME_SIZE", sizeof(context_frame));
	define("CONTEXT_FRAME_RA", offsetof(context_frame, ra));
	define("CONTEXT_FRAME_S0", offsetof(context_frame, s[0]));
	define("CONTEXT_FRAME_S1", offsetof(context_frame, s[1]));
	define("CONTEXT_FRAME_S2", offsetof(context_frame, s[2]));
	define("CONTEXT_FRAME_S3", offsetof(context_frame, s[3]));
	define("CONTEXT_FRAME_S4", offsetof(context_frame, s[4]));
	define("CONTEXT_FRAME_S5", offsetof(context_frame, s[5]));
	define("CONTEXT_FRAME_S6", offsetof(context_frame, s[6]));
	define("CONTEXT_FRAME_S7", offsetof(context_frame, s[7]));
	define("CONTEXT_FRAME_S8", offsetof(context_frame, s[8]));
	define("CONTEXT_FRAME_S9", offsetof(context_frame, s[9]));
	define("CONTEXT_FRAME_S10", offsetof(context_frame, s[10]));
	define("CONTEXT_FRAME_S11", offsetof(context_frame, s[11]));
	text();
	text("/* trap_frame */");
	define("TRAP_FRAME_SIZE", sizeof(trap_frame));
	define("TRAP_FRAME_RA", offsetof(trap_frame, ra));
	define("TRAP_FRAME_GP", offsetof(trap_frame, gp));
	define("TRAP_FRAME_A0", offsetof(trap_frame, a[0]));
	define("TRAP_FRAME_A1", offsetof(trap_frame, a[1]));
	define("TRAP_FRAME_A2", offsetof(trap_frame, a[2]));
	define("TRAP_FRAME_A3", offsetof(trap_frame, a[3]));
	define("TRAP_FRAME_A4", offsetof(trap_frame, a[4]));
	define("TRAP_FRAME_A5", offsetof(trap_frame, a[5]));
	define("TRAP_FRAME_A6", offsetof(trap_frame, a[6]));
	define("TRAP_FRAME_A7", offsetof(trap_frame, a[7]));
	define("TRAP_FRAME_S0", offsetof(trap_frame, s[0]));
	define("TRAP_FRAME_S1", offsetof(trap_frame, s[1]));
	define("TRAP_FRAME_S2", offsetof(trap_frame, s[2]));
	define("TRAP_FRAME_S3", offsetof(trap_frame, s[3]));
	define("TRAP_FRAME_S4", offsetof(trap_frame, s[4]));
	define("TRAP_FRAME_S5", offsetof(trap_frame, s[5]));
	define("TRAP_FRAME_S6", offsetof(trap_frame, s[6]));
	define("TRAP_FRAME_S7", offsetof(trap_frame, s[7]));
	define("TRAP_FRAME_S8", offsetof(trap_frame, s[8]));
	define("TRAP_FRAME_S9", offsetof(trap_frame, s[9]));
	define("TRAP_FRAME_S10", offsetof(trap_frame, s[10]));
	define("TRAP_FRAME_S11", offsetof(trap_frame, s[11]));
	define("TRAP_FRAME_T0", offsetof(trap_frame, t[0]));
	define("TRAP_FRAME_T1", offsetof(trap_frame, t[1]));
	define("TRAP_FRAME_T2", offsetof(trap_frame, t[2]));
	define("TRAP_FRAME_T3", offsetof(trap_frame, t[3]));
	define("TRAP_FRAME_T4", offsetof(trap_frame, t[4]));
	define("TRAP_FRAME_T5", offsetof(trap_frame, t[5]));
	define("TRAP_FRAME_T6", offsetof(trap_frame, t[6]));
	define("TRAP_FRAME_xEPC", offsetof(trap_frame, xepc));
	define("TRAP_FRAME_xTVAL", offsetof(trap_frame, xtval));
	define("TRAP_FRAME_xSTATUS", offsetof(trap_frame, xstatus));
	define("TRAP_FRAME_TP", offsetof(trap_frame, tp));
	define("TRAP_FRAME_SP", offsetof(trap_frame, sp));
	text();
	text("/* thread */");
	define("THREAD_CTX_KSP", offsetof(thread, ctx.ksp));
	define("THREAD_CTX_KSTACK", offsetof(thread, ctx.kstack));
	define("THREAD_CTX_XSP", offsetof(thread, ctx.xsp));
	define("THREAD_CTX_IRQ_NESTING", offsetof(thread, ctx.irq_nesting));
	text();
	text("/* machine status register */");
	mstatus mstatus_;
	define("MSTATUS_MPP", mstatus_.MPP.mask);
	define("MSTATUS_MIE", mstatus_.MIE.mask);
	text();
	text("/* machine interrupt enable register */");
	mie mie_;
	define("MIE_MEIE", mie_.MEIE.mask);
	define("MIE_MTIE", mie_.MTIE.mask);
}
