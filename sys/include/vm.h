#ifndef vm_h
#define vm_h

#include <page.h>

struct as;
struct seg;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Kernel interface
 */
void	    vm_init(void);
void	    vm_dump(void);
struct as  *as_create(pid_t);
struct as  *as_copy(struct as *, pid_t);
void	    as_destroy(struct as *);
void	    as_reference(struct as *);
int	    as_read(struct as *, void *, const void *, size_t);
int	    as_write(struct as *, const void *, void *, size_t);
void	    as_switch(struct as*);
void	    as_dump(struct as *);

#if defined(__cplusplus)
} /* extern "C" */

#include <memory>

struct vnode;

/*
 * MMU interface
 */
void   *as_map(as *, void *, size_t, int, int, std::unique_ptr<vnode>,
	       off_t, MEM_TYPE);
int	as_unmap(as *, void *, size_t, vnode *, off_t);
int	as_mprotect(as *, void *, size_t, int);
int	as_insert(as *, std::unique_ptr<phys>, size_t, int, int,
		  std::unique_ptr<vnode>, off_t, MEM_TYPE);

namespace std {

template<>
struct default_delete<as> {
	void operator()(as *a) { as_destroy(a); }
};

} /* namespace std */

#endif /* __cplusplus */

#endif /* vm_h */
