#include <sys/prctl.h>

#include <access.h>
#include <debug.h>
#include <errno.h>
#include <mutex>
#include <stdarg.h>
#include <thread.h>

namespace {

int
pr_set_name(const char *uname)
{
	interruptible_lock l(u_access_lock);
	if (auto r = l.lock(); r < 0)
		return r;
	if (!u_strcheck(uname, 16))
		return DERR(-EFAULT);
	thread_name(thread_cur(), uname);
	return 0;
}

}

/*
 * prctl
 */
int prctl(int op, ...)
{
	unsigned long x[4];
	va_list ap;
	va_start(ap, op);
	for (int i = 0; i < 4; ++i)
		x[i] = va_arg(ap, unsigned long);
	va_end(ap);

	switch (op) {
	case PR_SET_NAME:
		return pr_set_name((char *)x[0]);
	default:
		dbg("WARNING: unimplemented prctl %d %lu %lu %lu %lu\n",
		    op, x[0], x[1], x[2], x[3]);
		return DERR(-ENOSYS);
	}
}
