#ifndef debug_h
#define debug_h

#include <conf/config.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <syslog.h>

struct thread;

#if defined(CONFIG_DEBUG)
#define dbg(...) syslog_printf(LOG_DEBUG, __VA_ARGS__)
#else
#define dbg(...)
#endif

#if defined(CONFIG_INFO)
#define info(...) syslog_printf(LOG_INFO, __VA_ARGS__)
#else
#define info(...)
#endif

#define notice(...) syslog_printf(LOG_NOTICE, __VA_ARGS__)
#define warning(...) syslog_printf(LOG_WARNING, __VA_ARGS__)
#define error(...) syslog_printf(LOG_ERR, __VA_ARGS__)
#define critical(...) syslog_printf(LOG_CRIT, __VA_ARGS__)
#define alert(...) syslog_printf(LOG_ALERT, __VA_ARGS__)
#define emergency(...) syslog_printf(LOG_EMERG, __VA_ARGS__)

#define DERR(err) ({ \
	dbg("%u:("#err")\n", __LINE__); \
	err; \
})

#if defined(__cplusplus)
#define noreturn [[noreturn]]
extern "C" {
#endif

noreturn void	panic(const char *);
void		backtrace();
void		backtrace_thread(struct thread *);
void		syslog_printf(int, const char *, ...)
			__attribute__((format (printf, 2, 3)));
void		syslog_output(void (*)(void));
int		syslog_format(char *, size_t);
void		syslog_panic(void);
int		sc_syslog(int, char *, int);

#if defined(__cplusplus)
} /* extern "C" */
#undef noreturn
#endif

#endif /* !debug_h */
