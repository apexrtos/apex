/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VM_H
#define _VM_H

#include <sys/cdefs.h>
#include <arch.h>	/* for pgd_t */

/*
 * One structure per allocated region.
 */
struct region {
	struct region	*prev;		/* region list sorted by address */
	struct region	*next;
	struct region	*sh_prev;	/* link for all shared region */
	struct region	*sh_next;
	void		*addr;		/* base address */
	size_t		size;		/* size */
	int		flags;		/* flag */
#ifdef CONFIG_MMU
	void		*phys;		/* physical address */
#endif
};

/* Flags for region */
#define REG_READ	0x00000001
#define REG_WRITE	0x00000002
#define REG_EXEC	0x00000004
#define REG_SHARED	0x00000008
#define REG_MAPPED	0x00000010
#define REG_FREE	0x00000080

/*
 * VM mapping per one task.
 */
struct vm_map {
	struct region	head;		/* list head of regions */
	int		refcnt;		/* reference count */
	pgd_t		pgd;		/* page directory */
};

/* VM attributes */
#define VMA_READ	0x01
#define VMA_WRITE	0x02
#define VMA_EXEC	0x04

__BEGIN_DECLS
int	 vm_allocate(task_t, void **, size_t, int);
int	 vm_free(task_t, void *);
int	 vm_attribute(task_t, void *, int);
int	 vm_map(task_t, void *, size_t, void **);
vm_map_t vm_fork(vm_map_t);
vm_map_t vm_create(void);
int	 vm_reference(vm_map_t);
void	 vm_terminate(vm_map_t);
void	 vm_switch(vm_map_t);
int	 vm_load(vm_map_t, struct module *, void **);
void	*vm_translate(void *, size_t);
void	 vm_dump(void);
void	 vm_init(void);
__END_DECLS

#endif /* !_VM_H */
