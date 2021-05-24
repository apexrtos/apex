#pragma once

/*
 * Wire debug to stdout for test harness.
 */

#include <cstdio>
#include <cstdarg>
#include <cstddef>

#define DERR(err) ({ \
	dbg("%u:("#err")\n", __LINE__); \
	err; \
})

static inline void
 __attribute__((format (printf, 1, 2)))
dbg(const char *fmt, ...)
{
#if defined(CONFIG_DEBUG)
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
#endif
}

static inline void
__attribute__((format (printf, 1, 2)))
info(const char *fmt, ...)
{
#if defined(CONFIG_INFO)
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
#endif
}

static inline void
__attribute__((format (printf, 1, 2)))
notice(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static inline void
__attribute__((format (printf, 1, 2)))
warning(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static inline void
__attribute__((format (printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static inline void
__attribute__((format (printf, 1, 2)))
critical(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static inline void
__attribute__((format (printf, 1, 2)))
alert(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static inline void
__attribute__((format (printf, 1, 2)))
emergency(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static bool panic_fail;
static inline void
panic(const char *str)
{
	printf("PANIC: %s\n", str);
	panic_fail = true;
}
