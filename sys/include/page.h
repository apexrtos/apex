#ifndef page_h
#define page_h

#include <types.h>

struct bootargs;

/*
 * Page allocation flags
 */
#define PAF_REALLOC 0x20000000		    /* extend existing allocation */
#define PAF_MAPPED 0x40000000		    /* page is part of a vm mapping */
#define PAF_NO_SPEED_FALLBACK 0x80000000    /* do not use alternate speed */
#define PAF_MASK 0xe0000000

struct meminfo {
	phys *base;		    /* start address */
	size_t size;		    /* size in bytes */
	unsigned long attr;	    /* bitfield of MA_... attributes */
};

#if defined(__cplusplus)
extern "C" {
#endif

phys *page_alloc_order(size_t order, unsigned long ma_paf, void *);
phys *page_alloc(size_t, unsigned long ma_paf, void *);
phys *page_reserve(phys *, size_t, unsigned long paf, void *);
int page_free(phys *, size_t, void *);
bool page_valid(const phys *, size_t, void *);
unsigned long page_attr(const phys *, size_t len);
void page_init(const struct meminfo *, size_t, const struct bootargs *);
void page_dump(void);

#if defined(__cplusplus)
} /* extern "C" */

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

#endif

#endif /* !page_h */
