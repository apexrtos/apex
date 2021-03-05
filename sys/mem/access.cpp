#include <access.h>
#include <u_string.h>

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

std::string_view
u_string(const char *u_str, const size_t maxlen)
{
	ssize_t len;
	if ((len = u_strnlen(u_str, maxlen)) < 0)
		return {};
	if ((size_t)len == maxlen)
		return {};
	return {u_str, (size_t)len};
}
