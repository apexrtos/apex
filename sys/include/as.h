#pragma once

/*
 * as.h - address space management
 */

#include <memory>
#include <page.h>
#include <sys/types.h>

struct as;
struct seg;
struct vnode;

as *as_create(pid_t);
as *as_copy(as *, pid_t);
void as_destroy(as *);
void as_reference(as *);
int as_transfer_begin(as *);
int as_transfer_begin_interruptible(as *);
void as_transfer_end(as *);
bool as_locked(as *);
int as_modify_begin(as *);
int as_modify_begin_interruptible(as *);
void as_modify_end(as *);
void as_switch(as *);
void as_dump(const as *);

/*
 * memory management interface
 */
void *as_map(as *, void *, size_t, int, int, std::unique_ptr<vnode>, off_t, long);
int as_unmap(as *, void *, size_t, vnode *, off_t);
int as_mprotect(as *, void *, size_t, int);
int as_madvise(as *, seg *, void *, size_t, int);
int as_insert(as *, page_ptr, size_t, int, int, std::unique_ptr<vnode>, off_t, long);

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
