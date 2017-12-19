/*
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

#ifndef _KMC_H
#define _KMC_H
/*
 * i8042 Keyboard Mouse Controller definition
 */

/* I/O ports */
#define KMC_DATA	0x60
#define KMC_PORTB	0x61
#define KMC_CMD		0x64
#define KMC_STS		0x64

/* Status */
#define STS_PERR	0x80	/* Parity error */
#define STS_TMO		0x40	/* Timeout */
#define STS_AUXOBF	0x20	/* Mouse OBF */
#define STS_INH		0x10	/* 0: inhibit  1: no-inhibit */
#define STS_SYS		0x04	/* 0: power up  1:Init comp */
#define STS_IBF		0x02	/* Input (to kbd) buffer full */
#define STS_OBF		0x01	/* Output (from kbd) buffer full */

/* Command */
#define CMD_READ_CMD	0x20
#define CMD_WRITE_CMD	0x60
#define CMD_AUX_DIS	0xa7
#define CMD_AUX_EN	0xa8
#define CMD_KBD_DIS	0xad
#define CMD_KBD_EN	0xae
#define CMD_READ_OPORT	0xd0
#define CMD_WRITE_OPORT	0xd1
#define CMD_WRITE_AUX	0xd4

/* Auxillary device command */
#define AUX_ENABLE	0xf4
#define AUX_DISABLE	0xf5
#define AUX_RESET	0xff

/*
 * wait_obf: Wait output buffer full
 * wait_obe: Wait input buffer empty
 * wait_ibf: Wait input buffer full
 * wait_ibe: Wait input buffer empty
 */
#define wait_obf()	while ((inb(KMC_STS) & STS_OBF) == 0)
#define wait_obe()	while (inb(KMC_STS) & STS_OBF)
#define wait_ibf()	while ((inb(KMC_STS) & STS_IBF) == 0)
#define wait_ibe()	while (inb(KMC_STS) & STS_IBF)

#endif /* !_KMC_H */
