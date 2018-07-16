#include <access.h>

bool
u_strcheck(const char *u_str, const size_t maxlen)
{
	ssize_t len;
	if ((len = u_strnlen(u_str, maxlen)) < 0)
		return false;
	if ((size_t)len == maxlen)
		return false;
	return true;
}
