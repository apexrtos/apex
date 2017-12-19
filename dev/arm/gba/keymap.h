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
 * keymap.h - Keyboard mapping
 */
#include <prex/keycode.h>

struct _key_info {
	int pos_x;	/* Cursor position */
	int width;	/* Cursor width */
	u_char normal;	/* Normal code */
	u_char shifted;	/* Shifted code */
};

const struct _key_info key_info[5][14] =
	{{{0,   9,  '`', '~'},
	  {8,   9,  '1', '!'},
	  {16,  9,  '2', '@'},
	  {24,  9,  '3', '#'},
	  {32,  9,  '4', '$'},
	  {40,  9,  '5', '%'},
	  {48,  9,  '6', '^'},
	  {56,  9,  '7', '&'},
	  {64,  9,  '8', '*'},
	  {72,  9,  '9', '('},
	  {80,  9,  '0', ')'},
	  {88,  9,  '-', '_'},
	  {96,  9,  '=', '+'},
	  {104, 15, '\b', '\b'}}, /* back space */

	 {{0,   15, '\t', '\t'}, /* tab */
	  {14,  9,  'q', 'Q'},
	  {22,  9,  'w', 'W'},
	  {30,  9,  'e', 'E'},
	  {38,  9,  'r', 'R'},
	  {46,  9,  't', 'T'},
	  {54,  9,  'y', 'Y'},
	  {62,  9,  'u', 'U'},
	  {70,  9,  'i', 'I'},
	  {78,  9,  'o', 'O'},
	  {86,  9,  'p', 'P'},
	  {94,  9,  '[', '{'},
	  {102, 9,  ']', '}'},
	  {110, 9,  '\\', '|'}},

	 {{0,   17, K_CAPS, K_CAPS},
	  {16,  9,  'a', 'A'},
	  {24,  9,  's', 'S'},
	  {32,  9,  'd', 'D'},
	  {40,  9,  'f', 'F'},
	  {48,  9,  'g', 'G'},
	  {56,  9,  'h', 'H'},
	  {64,  9,  'j', 'J'},
	  {72,  9,  'k', 'K'},
	  {80,  9,  'l', 'L'},
	  {88,  9,  ';', ':'},
	  {96,  9,  '\'','"'},
	  {104, 15, '\n', '\n'},
	  {0,   0,  0,   0}}, /* dummy */

	 {{0,   19, K_SHFT, K_SHFT},
	  {18,  9,  'z', 'Z'},
	  {26,  9,  'x', 'X'},
	  {34,  9,  'c', 'C'},
	  {42,  9,  'v', 'V'},
	  {50,  9,  'b', 'B'},
	  {58,  9,  'n', 'N'},
	  {66,  9,  'm', 'M'},
	  {74,  9,  ',', '<'},
	  {82,  9,  '.', '>'},
	  {90,  9,  '/', '?'},
	  {98,  11, K_PGUP, K_PGUP}, /* special */
	  {108, 11, K_PGDN, K_PGDN}, /* special */
	  {0,   0,  0,   0}}, /* dummy */

	 {{0,   12, 0x1b, 0x1b}, /* escape */
	  {11,  12, K_INS, K_INS},
	  {22,  13, 0x7f, 0x7f}, /* delete */
	  {34,  53, ' ', ' '},
	  {34,  53, ' ', ' '},
	  {34,  53, ' ', ' '},
	  {34,  53, ' ', ' '},
	  {34,  53, ' ', ' '},
	  {34,  53, ' ', ' '},
	  {86,  9,  K_DOWN, K_DOWN},
	  {94,  9,  K_UP,   K_UP  },
	  {102, 9,  K_LEFT, K_LEFT},
	  {110, 9,  K_RGHT, K_RGHT},
	  {0,   0,  0,   0}}, /* dummy */
};

/* Maximum colum number for each row */
u_int max_x[] = {13,13,12,12,12};

