#include <new>

#include <debug.h>
#include <kmem.h>

/*
 * Simple operator new
 */
void *
operator new(const std::size_t s)
{
	const auto p = kmem_alloc(s, MEM_NORMAL);
	if (!p)
		panic("OOM");
	return p;
}
