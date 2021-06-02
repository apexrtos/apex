#include <access.h>
#include <u_string.h>

#include <kernel.h>
#include <page.h>
#include <seg.h>
#include <task.h>

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

bool
u_access_ok(const void *u_addr, size_t len, int access)
{
	return u_access_okfor(task_cur()->as, u_addr, len, access);
}

bool
k_access_ok(const void *k_addr, size_t len, int access)
{
	return page_valid(virt_to_phys(k_addr), len, &kern_task);
}

bool
k_address(const void *k_addr)
{
	return page_valid(virt_to_phys(k_addr), 0, &kern_task);
}

bool
u_address(const void *u_addr)
{
	return u_addressfor(task_cur()->as, u_addr);
}

bool
u_addressfor(const as *a, const void *u_addr)
{
	return as_find_seg(a, u_addr).ok();
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
