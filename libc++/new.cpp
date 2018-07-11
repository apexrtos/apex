#include <new>

#include <debug.h>
#include <kmem.h>

/*
 * Simple operator new
 */
void *
operator new(const std::size_t s)
{
	return kmem_alloc(s, MEM_NORMAL);
}
