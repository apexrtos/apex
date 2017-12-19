/*-
 * Copyright (c) 2007, Kohsuke Ohtani
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

#include <driver.h>

extern struct driver console_drv;
extern struct driver cpu_drv;
extern struct driver cpufreq_drv;
extern struct driver fdd_drv;
extern struct driver kbd_drv;
extern struct driver keypad_drv;
extern struct driver null_drv;
extern struct driver mouse_drv;
extern struct driver pm_drv;
extern struct driver ramdisk_drv;
extern struct driver rtc_drv;
extern struct driver tty_drv;
extern struct driver zero_drv;
extern struct driver serial_drv;

/*
 * Driver table
 */
struct driver *driver_table[] = {
#ifdef CONFIG_CONSOLE
	&console_drv,
#endif
#ifdef CONFIG_CPUFREQ
	&cpu_drv,
	&cpufreq_drv,
#endif
#ifdef CONFIG_FDD
	&fdd_drv,
#endif
#ifdef CONFIG_KEYBOARD
	&kbd_drv,
#endif
#ifdef CONFIG_KEYPAD
	&keypad_drv,
#endif
#ifdef CONFIG_NULL
	&null_drv,
#endif
#ifdef CONFIG_MOUSE
	&mouse_drv,
#endif
#ifdef CONFIG_PM
	&pm_drv,
#endif
#ifdef CONFIG_RAMDISK
	&ramdisk_drv,
#endif
#ifdef CONFIG_RTC
	&rtc_drv,
#endif
#ifdef CONFIG_TTY
	&tty_drv,
#endif
#ifdef CONFIG_ZERO
	&zero_drv,
#endif
#ifdef CONFIG_SERIAL
	&serial_drv,
#endif
	NULL
};
