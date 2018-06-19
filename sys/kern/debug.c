/*
 * debug.c - kernel debug services
 */

#include <debug.h>

#include <arch.h>
#include <thread.h>

/*
 * panic - print panic message and halt system
 */
noreturn void
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
backtrace(void)
{
	backtrace_thread(thread_cur());
}

/*
 * backtrace_thread - print backtrace of specified thread
 */
void
backtrace_thread(struct thread *th)
{
	arch_backtrace(th);
}

/*
 * __assert_fail - print assertion message and halt system
 */
noreturn void
__assert_fail(const char *expr, const char *file, int line, const char *func)
{
	interrupt_disable();
	backtrace();
	emergency("Assertion failed: %s (%s: %s: %d)\n",
	    expr, file, func, line);
	syslog_panic();
	machine_panic();
}
