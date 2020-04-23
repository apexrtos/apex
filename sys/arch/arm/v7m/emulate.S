/*
 * emulate.S - instruction emulation
 */

.syntax unified

#include <asm.h>
#include <asm_def.h>
#include <cpu.h>

/*
 * exc_emulate - ARMv7-M instruction emulation
 *
 * Emulate hardware TLS support for ARMv7-M
 */
.section .fast_text, "ax"
.thumb_func
.global v7m_emulate
v7m_emulate:
	/*
	 * Figure out which stack the exception frame is on:
	 * If we are in thread mode SPSEL is 1 (process stack)
	 * If we are in handler mode SPSEL is 0 (main stack)
	 */
	tst lr, EXC_SPSEL
	mrsne r0, psp
	moveq r0, sp			/* r0 = exception frame */

	/* emulate mrc 15, 0, rX, cr13, cr0, {3} */
	ldr r3, [r0, EFRAME_RA]         /* r3 = address of failed instruction */
	ldr r1, [r3]			/* r1 = failed instruction */
	movw r2, :lower16:0x0f70ee1d
	movt r2, :upper16:0x0f70ee1d    /* r2 = instruction mask */
	bics r2, r1			/* check for mrc get_tls */
	bne v7m_entry
	ubfx r1, r1, 28, 4		/* r1 = destination reg */
	cmp r1, 14
	bhi v7m_entry
	movw r2, :lower16:active_thread
	movt r2, :upper16:active_thread /* r2 = &active_thread */
	ldr r2, [r2]			/* r2 = active_thread */
	ldr r2, [r2, THREAD_CTX_TLS]	/* r2 = tls pointer */

	/* skip emulated instruction */
	add r3, 4
	str r3, [r0, EFRAME_RA]

	/*
	 * Set destination register
	 *
	 * r0 points to exception frame,
	 * r1 is destination register, 0-14,
	 * r2 is value to set
	 */
0:	tbb [pc, r1]
	.byte (0f-0b-4)/2
	.byte (1f-0b-4)/2
	.byte (2f-0b-4)/2
	.byte (3f-0b-4)/2
	.byte (4f-0b-4)/2
	.byte (5f-0b-4)/2
	.byte (6f-0b-4)/2
	.byte (7f-0b-4)/2
	.byte (8f-0b-4)/2
	.byte (9f-0b-4)/2
	.byte (10f-0b-4)/2
	.byte (11f-0b-4)/2
	.byte (12f-0b-4)/2
	.byte (13f-0b-4)/2
	.byte (14f-0b-4)/2
	.byte 0
0:	str r2, [r0, EFRAME_R0]
	bx lr
1:	str r2, [r0, EFRAME_R1]
	bx lr
2:	str r2, [r0, EFRAME_R2]
	bx lr
3:	str r2, [r0, EFRAME_R3]
	bx lr
4:	mov r4, r2
	bx lr
5:	mov r5, r2
	bx lr
6:	mov r6, r2
	bx lr
7:	mov r7, r2
	bx lr
8:	mov r8, r2
	bx lr
9:	mov r9, r2
	bx lr
10:	mov r10, r2
	bx lr
11:	mov r11, r2
	bx lr
12:	str r2, [r0, EFRAME_R12]
	bx lr
13:	msr psp, r2
	bx lr
14:	str r2, [r0, EFRAME_LR]
	bx lr

