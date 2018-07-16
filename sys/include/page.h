#ifndef page_h
#define page_h

#include <types.h>

struct bootinfo;

enum PAGE_ALLOC_TYPE {
	PAGE_ALLOC_FIXED,	/* Page must remain fixed in memory */
	PAGE_ALLOC_MAPPED,	/* Page is part of a vm mapping */
};

#if defined(__cplusplus)
extern "C" {
#endif

phys   *page_alloc_order(size_t order, enum MEM_TYPE, enum PAGE_ALLOC_TYPE, void *);
phys   *page_alloc(size_t, enum MEM_TYPE, enum PAGE_ALLOC_TYPE, void *);
phys   *page_reserve(phys *, size_t, void *);
int	page_free(phys *, size_t, void *);
bool	page_valid(phys *, size_t, void *);
void	page_init(const struct bootinfo *, void *);
void	page_dump(void);

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
