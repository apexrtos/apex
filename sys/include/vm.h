#ifndef vm_h
#define vm_h

#include <page.h>

struct as;
struct iovec;
struct seg;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Kernel interface
 */
void		    vm_init(void);
void		    vm_dump(void);
void		    vm_init_brk(struct as *, void *);
ssize_t		    vm_readv(struct as *, const struct iovec *, size_t,
			     const struct iovec *, size_t);
ssize_t		    vm_writev(struct as *, const struct iovec *, size_t,
			      const struct iovec *, size_t);
ssize_t		    vm_read(struct as *, void *, const void *, size_t);
ssize_t		    vm_write(struct as *, const void *, void *, size_t);
struct as	   *as_create(pid_t);
struct as	   *as_copy(struct as *, pid_t);
void		    as_destroy(struct as *);
void		    as_reference(struct as *);
int		    as_transfer_begin(struct as *);
int		    as_transfer_end(struct as *);
int		    as_modify_begin(struct as *);
int		    as_modify_end(struct as *);
void		    as_switch(struct as *);
void		    as_dump(struct as *);
const struct seg   *as_find_seg(const struct as *, const void *);
void		   *seg_begin(const struct seg *);
void		   *seg_end(const struct seg *);
size_t		    seg_size(const struct seg *);
int		    seg_prot(const struct seg *);

#if defined(__cplusplus)
} /* extern "C" */

#include <memory>

struct vnode;

/*
 * MMU interface
 */
void	   *as_map(as *, void *, size_t, int, int, std::unique_ptr<vnode>,
		   off_t, long mem_attr);
int	    as_unmap(as *, void *, size_t, vnode *, off_t);
int	    as_mprotect(as *, void *, size_t, int);
int	    as_insert(as *, std::unique_ptr<phys>, size_t, int, int,
		      std::unique_ptr<vnode>, off_t, long mem_attr);

namespace std {

template<>
struct default_delete<as> {
	void operator()(as *a)
	{
		as_modify_begin(a);
		as_destroy(a);
	}
};

} /* namespace std */

#endif /* __cplusplus */

#endif /* vm_h */
