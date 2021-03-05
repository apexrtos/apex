/*
 * debug.c - kernel debug services
 */

#include <debug.h>

#include <arch/backtrace.h>
#include <arch/interrupt.h>
#include <arch/machine.h>
#include <cassert>
#include <thread.h>

/*
 * dbg - log debug message to system log
 */
void
dbg(const char *fmt, ...)
{
#if defined(CONFIG_DEBUG)
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_DEBUG, fmt, ap);
	va_end(ap);
#endif
}

/*
 * info - log informative message to system log
 */
void
info(const char *fmt, ...)
{
#if defined(CONFIG_INFO)
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_INFO, fmt, ap);
	va_end(ap);
#endif
}

/*
 * notice - log notice to system log
 */
void
notice(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_NOTICE, fmt, ap);
	va_end(ap);
}

/*
 * warning - log warning to system log
 */
void
warning(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_WARNING, fmt, ap);
	va_end(ap);
}

/*
 * error - log error to system log
 */
void
error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_ERR, fmt, ap);
	va_end(ap);
}

/*
 * critical - log critical message to system log
 */
void
critical(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_CRIT, fmt, ap);
	va_end(ap);
}

/*
 * alert - log alert to system log
 */
void
alert(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_ALERT, fmt, ap);
	va_end(ap);
}

/*
 * emergency - log emergency message to system log
 */
void
emergency(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	syslog_vprintf(LOG_EMERG, fmt, ap);
	va_end(ap);
}

/*
 * panic - print panic message and halt system
 */
[[noreturn]] void
panic(const char *msg)
{
	interrupt_disable();
	backtrace();
	emergency("PANIC: %s\n", msg);
	syslog_panic();
	machine_panic();
}

/*
 * backtrace - print backtrace of current thread
 */
void
backtrace()
{
	backtrace_thread(thread_cur());
}

/*
 * backtrace_thread - print backtrace of specified thread
 */
void
backtrace_thread(thread *th)
{
	arch_backtrace(th);
}

/*
 * __assert_fail - print assertion message and halt system
 */
extern "C" _Noreturn void
__assert_fail(const char *expr, const char *file, int line, const char *func)
{
	interrupt_disable();
	backtrace();
	emergency("Assertion failed: %s (%s: %s: %d)\n",
	    expr, file, func, line);
	syslog_panic();
	machine_panic();
}

extern "C" _Noreturn void
abort()
{
	panic("abort");
}
