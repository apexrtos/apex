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

#ifndef _PREX_IOCTL_H
#define _PREX_IOCTL_H

#include <sys/time.h>

/*
 * CPU I/O control code
 */
#define CPUIOC_GET_INFO		_IOR('6', 0, struct cpu_info)
#define CPUIOC_GET_STAT		_IOR('6', 1, struct cpu_stat)

/*
 * CPU information
 */
struct cpu_info {
	unsigned int id;	/* processor id */
	char name[50];		/* name string */
	int speed;		/* max speed in MHz */
	int power;		/* max power in mV */
	int clock_ctrl; 	/* true if it supports clock control */
};

/*
 * Current status
 */
struct cpu_stat {
	int speed;		/* speed in MHz */
	int power;		/* power in mVolt */
};

/*
 * Power management I/O control code
 */
#define PMIOC_SET_POWER		_IOW('P', 0, int)
#define PMIOC_SET_TIMER		_IOW('P', 1, int)
#define PMIOC_GET_TIMER		_IOR('P', 2, int)
#define PMIOC_SET_POLICY	_IOW('P', 3, int)
#define PMIOC_GET_POLICY	_IOR('P', 4, int)

/*
 * Power Management Policy
 */
#define PM_PERFORMANCE		0
#define PM_POWERSAVE		1

/*
 * Power state
 */
#define POWER_ON		0
#define POWER_SUSPEND		1
#define POWER_OFF		2
#define POWER_REBOOT		3

/*
 * RTC I/O control code
 */
#define RTCIOC_GET_TIME		_IOR('R', 0, struct __timeval)
#define RTCIOC_SET_TIME		_IOR('R', 1, struct __timeval)

struct __timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

#endif /* !_PREX_IOCTL_H */
