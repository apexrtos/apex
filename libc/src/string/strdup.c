#include <stdlib.h>
#include <string.h>
#include "libc.h"

#include <kmem.h>

char *__strdup(const char *s)
{
	size_t l = strlen(s);
	char *d = kmem_alloc(l+1, MEM_NORMAL);
	if (!d) return NULL;
	return memcpy(d, s, l+1);
}

weak_alias(__strdup, strdup);
