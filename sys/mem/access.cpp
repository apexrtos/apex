#include <access.h>

#include <debug.h>
#include <errno.h>
#include <limits>
#include <string.h>

extern "C" ssize_t
u_strnlen(const char *u_str, const size_t maxlen)
{
	if (maxlen > std::numeric_limits<ssize_t>::max())
		return DERR(-EINVAL);
	const auto r = strnlen(u_str, maxlen);
	if (u_fault())
		return DERR(-EFAULT);
	return r;
}

extern "C" ssize_t
u_arraylen(const void **u_arr, const size_t maxlen)
{
	if (maxlen > std::numeric_limits<ssize_t>::max())
		return DERR(-EINVAL);
	size_t i;
	for (i = 0; i < maxlen && u_arr[i]; ++i);
	if (u_fault())
		return DERR(-EFAULT);
	return i;
}

extern "C" bool
u_strcheck(const char *u_str, const size_t maxlen)
{
	ssize_t len;
	if ((len = u_strnlen(u_str, maxlen)) < 0)
		return false;
	if ((size_t)len == maxlen)
		return false;
	return true;
}
