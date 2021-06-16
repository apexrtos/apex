#pragma once

#include <address.h>
#include <lib/expect.h>
#include <memory>

struct bootargs;

/*
 * Page allocation flags
 */
#define PAF_REALLOC 0x20000000		    /* extend existing allocation */
#define PAF_MAPPED 0x40000000		    /* page is part of a vm mapping */
#define PAF_EXACT_SPEED 0x80000000	    /* do not allow alternate speed */
#define PAF_MASK 0xe0000000

/*
 * Memory information
 */
struct meminfo {
	phys base;		    /* start address */
	size_t size;		    /* size in bytes */
	unsigned long attr;	    /* bitfield of MA_... attributes */
	unsigned priority;	    /* lowest priority allocated first */
};

/*
 * page_ptr - like unique_ptr, but for pages
 */
class page_ptr {
public:
	page_ptr();
	page_ptr(page_ptr &&);
	page_ptr(phys p, size_t size, void *owner);

	~page_ptr();

	page_ptr &operator=(page_ptr &&);

	phys release();
	void reset();
	phys get();
	size_t size() const;
	explicit operator bool() const;

private:
	phys phys_;
	size_t size_;
	void *owner_;
};

page_ptr page_alloc_order(size_t order, unsigned long ma_paf, void *);
page_ptr page_alloc(size_t, unsigned long ma_paf, void *);
page_ptr page_reserve(phys, size_t, unsigned long paf, void *);
expect_ok page_free(phys, size_t, void *);
bool page_valid(const phys, size_t, void *);
expect_pos page_attr(const phys, size_t len);
void page_init(const meminfo *, size_t, const bootargs *);
void page_dump();

/*
 * Address translation
 */
static inline void *
phys_to_virt(page_ptr &p)
{
	return phys_to_virt(p.get());
}
