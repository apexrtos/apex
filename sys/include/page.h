#pragma once

#include <address.h>
#include <memory>

struct bootargs;

/*
 * Page allocation flags
 */
#define PAF_REALLOC 0x20000000		    /* extend existing allocation */
#define PAF_MAPPED 0x40000000		    /* page is part of a vm mapping */
#define PAF_EXACT_SPEED 0x80000000	    /* do not allow alternate speed */
#define PAF_MASK 0xe0000000

struct meminfo {
	phys base;		    /* start address */
	size_t size;		    /* size in bytes */
	unsigned long attr;	    /* bitfield of MA_... attributes */
	unsigned priority;	    /* lowest priority allocated first */
};


phys page_alloc_order(size_t order, unsigned long ma_paf, void *);
phys page_alloc(size_t, unsigned long ma_paf, void *);
phys page_reserve(phys, size_t, unsigned long paf, void *);
int page_free(phys, size_t, void *);
bool page_valid(const phys, size_t, void *);
unsigned long page_attr(const phys, size_t len);
void page_init(const meminfo *, size_t, const bootargs *);
void page_dump();

#include <memory>

namespace std {

template<>
struct default_delete<phys> {
	default_delete(size_t size, void *owner)
	: size_{size}
	, owner_{owner}
	{}

	void operator()(phys* p) const
	{
		page_free(p, size_, owner_);
	}
private:
	size_t size_;
	void *owner_;
};

} /* namespace std */

