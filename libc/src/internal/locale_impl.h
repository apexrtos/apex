#ifndef _LOCALE_IMPL_H
#define _LOCALE_IMPL_H

#include <locale.h>
#include <stdlib.h>
#include "libc.h"

#define LOCALE_NAME_MAX 23

struct __locale_map {
	const void *map;
	size_t map_size;
	char name[LOCALE_NAME_MAX+1];
	const struct __locale_map *next;
};

extern hidden const struct __locale_map __c_dot_utf8;
extern hidden const struct __locale_struct __c_locale;
extern hidden const struct __locale_struct __c_dot_utf8_locale;

hidden const struct __locale_map *__get_locale(int, const char *);
hidden const char *__mo_lookup(const void *, size_t, const char *);
hidden const char *__lctrans(const char *, const struct __locale_map *);
hidden const char *__lctrans_cur(const char *);
hidden const char *__lctrans_impl(const char *, const struct __locale_map *);
hidden int __loc_is_allocated(locale_t);
hidden char *__gettextdomain(void);

#define LOC_MAP_FAILED ((const struct __locale_map *)-1)

#define LCTRANS(msg, lc, loc) __lctrans(msg, (loc)->cat[(lc)])
#define LCTRANS_CUR(msg) __lctrans_cur(msg)

#define C_LOCALE ((locale_t)&__c_locale)
#define UTF8_LOCALE ((locale_t)&__c_dot_utf8_locale)

/* Apex kernel uses C locale only. */
#define CURRENT_UTF8 0

#undef MB_CUR_MAX
#define MB_CUR_MAX (CURRENT_UTF8 ? 4 : 1)

#endif
