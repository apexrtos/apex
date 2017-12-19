/*-
 * Copyright (c) 2005, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * lcd.h - GBA LCD definition
 */

/**
 * Memo: Map/Tile Location
 *
 *  BG1 ... Keyboard1 (normal)
 *  BG2 ... Keyboard2 (shifted)
 *  BG3 ... Text
 *
 *  +------------+ 6010000
 *  |            |
 *  |            |
 *  |            |
 *  |            |
 *  |            |
 *  +------------+ 600C000
 *  |  BG2 MAP   |
 *  +------------+ 600A000
 *  |  BG1 MAP   |
 *  +------------+ 6009000
 *  |  BG3 MAP   |
 *  +------------+ 6008000
 *  |   Tile 2   |
 *  |(KBD2)      |
 *  |- - - - - - | 6006000
 *  |            |
 *  |(KBD1)      |
 *  +------------+ 6004000
 *  |   Tile 1   |
 *  |            |
 *  |            |
 *  |            |
 *  |(Font)      |
 *  +------------+ 6000000
 */

#define VSCR_COLS	32
#define SCR_COLS	30
#define SCR_ROWS	20

/* Registers for keypad control */
#define REG_DISPCNT	(*(volatile uint16_t *)0x4000000)
#define REG_BG0CNT	(*(volatile uint16_t *)0x4000008)
#define REG_BG1CNT	(*(volatile uint16_t *)0x400000A)
#define REG_BG2CNT	(*(volatile uint16_t *)0x400000C)
#define REG_BG3CNT	(*(volatile uint16_t *)0x400000E)

#define BG_PALETTE	(uint16_t *)0x5000000

#define CONSOLE_TILE	(uint16_t *)0x6000000
#define CONSOLE_MAP	(uint16_t *)0x6008000

#define KBD1_TILE	(uint16_t *)0x6004000
#define KBD2_TILE	(uint16_t *)0x6006000
#define KBD1_MAP	(uint16_t *)0x6009000
#define KBD2_MAP	(uint16_t *)0x600A000

#define OAM		(uint16_t *)0x7000000
#define SPL_PALETTE	(uint16_t *)0x5000200
#define CURSOR_DATA	(uint16_t *)0x6010000


#define	RGB(r, g, b)	(((b) << 10) + ((g) << 5) + (r))
