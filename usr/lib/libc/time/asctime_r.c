/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

#include <sys/time.h>
#include <stdio.h>
#include <time.h>

/*
** A la ISO/IEC 9945-1, ANSI/IEEE Std 1003.1, Second Edition, 1996-07-12.
*/

char *
asctime_r(timeptr, buf)
register const struct tm *	timeptr;
char *				buf;
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	const char *wn;
	const char *mn;

	wn = wday_name[timeptr->tm_wday];
	mn = mon_name[timeptr->tm_mon];
	/*
	** The X3J11-suggested format is
	**	"%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n"
	** Since the .2 in 02.2d is ignored, we drop it.
	*/
	(void)sprintf(buf,
		"%.3s %.3s%3d %02d:%02d:%02d %d\n",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		1900 + timeptr->tm_year);
	return buf;
}
