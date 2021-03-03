#pragma once

#include <conf/config.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <syslog.h>

struct thread;

#define DERR(err) ({ \
	dbg("%u:("#err")\n", __LINE__); \
	err; \
})

#if defined(__cplusplus)
#define noreturn [[noreturn]]
extern "C" {
#endif

void		dbg(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		info(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		notice(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		warning(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		error(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		critical(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		alert(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
void		emergency(const char *fmt, ...)
			__attribute__((format (printf, 1, 2)));
noreturn void	panic(const char *);
void		backtrace();
void		backtrace_thread(struct thread *);
int		syslog_printf(int, const char *, ...)
			__attribute__((format (printf, 2, 3)));
int		syslog_vprintf(int, const char *, va_list);
void		syslog_output(void (*)(void));
int		syslog_format(char *, size_t);
void		syslog_panic(void);
int		sc_syslog(int, char *, int);
void		kmsg_init();

#if defined(__cplusplus)
} /* extern "C" */
#undef noreturn
#endif
