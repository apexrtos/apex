#include "exception_frame.h"
#include <stddef.h>
#include <thread.h>

#define text(t) __asm__("\n@__OUT__" t)
#define define(t, v) __asm__("\n@__OUT__#define " t " %0" :: "n" (v))

void asm_def(void)
{
	text("#ifndef arm_v7m_asm_def_h");
	text("#define arm_v7m_asm_def_h");
	text();
	text("/*");
	text(" * asm_def.h - Automatically generated file. Do not edit.");
	text(" */");
	text();
	text("/* struct exception_frame */");
	define("EFRAME_R0", offsetof(struct exception_frame_basic, r0));
	define("EFRAME_R1", offsetof(struct exception_frame_basic, r1));
	define("EFRAME_R2", offsetof(struct exception_frame_basic, r2));
	define("EFRAME_R3", offsetof(struct exception_frame_basic, r3));
	define("EFRAME_R12", offsetof(struct exception_frame_basic, r12));
	define("EFRAME_LR", offsetof(struct exception_frame_basic, lr));
	define("EFRAME_RA", offsetof(struct exception_frame_basic, ra));
	define("EFRAME_XPSR", offsetof(struct exception_frame_basic, xpsr));
	define("EFRAME_BASIC_SIZE", sizeof(struct exception_frame_basic));
	define("EFRAME_EXTENDED_SIZE", sizeof(struct exception_frame_extended));
	text();
	text("/* struct thread */");
	define("THREAD_CTX_TLS", offsetof(struct thread, ctx.tls));
	define("THREAD_CTX_USP", offsetof(struct thread, ctx.usp));
	define("THREAD_CTX_KSTACK", offsetof(struct thread, ctx.kstack));
	define("THREAD_CTX_KSP", offsetof(struct thread, ctx.ksp));
	text();
	text("#endif");
}

