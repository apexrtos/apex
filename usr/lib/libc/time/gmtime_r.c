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

#include <sys/time.h>
#include <tzfile.h>

static const int daysinmonth[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int
is_leap(int year)
{
	return (isleap(year) ? 1 : 0);
}

struct tm *
gmtime_r(const time_t *timep, struct tm *tmp)
{
	time_t t;
	int days, year, mon;
	int i;

	t = *timep;
	days = t / 60 / 60 / 24;

	for (year = 1970, i = DAYSPERNYEAR + is_leap(year);
	     days >= i;
	     year++, i = DAYSPERNYEAR + is_leap(year))
		days -= i;

	tmp->tm_year = year - 1900;
	tmp->tm_yday = days;

	for (mon = 0; ; mon++) {
		i = daysinmonth[mon];
		if (mon == 1 && is_leap(year))
			i++;
		if (days < i)
			break;
		days -= i;
	}
	tmp->tm_mon = mon;
	tmp->tm_mday = days + 1;
	tmp->tm_wday = (int) ((1970 + days) % DAYSPERWEEK);

	tmp->tm_sec = t % 60;
	tmp->tm_min = (t / 60) % 60;
	tmp->tm_hour = (t / 60 / 60) % 24;
	tmp->tm_isdst = 0;
	return tmp;
}
