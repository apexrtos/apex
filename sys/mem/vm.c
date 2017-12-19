/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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

/*
 * vm.c - virtual memory allocator
 */

/*
 * A task owns its private virtual address space. All threads in
 * a task share one same memory space.
 * When new task is made, the address mapping of the parent task
 * is copied to child task's. In this time, the read-only space
 * is shared with old map.
 *
 * Since this kernel does not do page out to the physical storage,
 * it is guaranteed that the allocated memory is always continuing
 * and existing. Thereby, a kernel and drivers can be constructed
 * very simply.
 */

#include <kernel.h>
#include <kmem.h>
#include <thread.h>
#include <page.h>
#include <task.h>
#include <sched.h>
#include <vm.h>

/* forward declarations */
static struct region *region_create(struct region *, void *, size_t);
static void region_delete(struct region *, struct region *);
static struct region *region_find(struct region *, void *, size_t);
static struct region *region_alloc(struct region *, size_t);
static void region_free(struct region *, struct region *);
static struct region *region_split(struct region *, struct region *,
				   void *, size_t);
static void region_init(struct region *);
static int do_allocate(vm_map_t, void **, size_t, int);
static int do_free(vm_map_t, void *);
static int do_attribute(vm_map_t, void *, int);
static int do_map(vm_map_t, void *, size_t, void **);
static vm_map_t	do_fork(vm_map_t);


/* vm mapping for kernel task */
static struct vm_map kern_map;

/**
 * vm_allocate - allocate zero-filled memory for specified address
 *
 * If "anywhere" argument is true, the "addr" argument will be
 * ignored.  In this case, the address of free space will be
 * found automatically.
 *
 * The allocated area has writable, user-access attribute by
 * default.  The "addr" and "size" argument will be adjusted
 * to page boundary.
 */
int
vm_allocate(task_t task, void **addr, size_t size, int anywhere)
{
	int err;
	void *uaddr;

	sched_lock();

	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (task != cur_task() && !task_capable(CAP_MEMORY)) {
		err = EPERM;
		goto out;
	}
	if (umem_copyin(addr, &uaddr, sizeof(uaddr))) {
		err = EFAULT;
		goto out;
	}
	if (anywhere == 0 && !user_area(*addr)) {
		err = EACCES;
		goto out;
	}

	err = do_allocate(task->map, &uaddr, size, anywhere);
	if (err == 0) {
		if (umem_copyout(&uaddr, addr, sizeof(uaddr)))
			err = EFAULT;
	}
 out:
	sched_unlock();
	return err;
}

static int
do_allocate(vm_map_t map, void **addr, size_t size, int anywhere)
{
	struct region *reg;
	char *start, *end, *phys;

	if (size == 0)
		return EINVAL;

	/*
	 * Allocate region
	 */
	if (anywhere) {
		size = (size_t)PAGE_ALIGN(size);
		if ((reg = region_alloc(&map->head, size)) == NULL)
			return ENOMEM;
	} else {
		start = (char *)PAGE_TRUNC(*addr);
		end = (char *)PAGE_ALIGN(start + size);
		size = (size_t)(end - start);

		reg = region_find(&map->head, start, size);
		if (reg == NULL || !(reg->flags & REG_FREE))
			return EINVAL;

		reg = region_split(&map->head, reg, start, size);
		if (reg == NULL)
			return ENOMEM;
	}
	reg->flags = REG_READ | REG_WRITE;

	/*
	 * Allocate physical pages, and map them into virtual address
	 */
	if ((phys = page_alloc(size)) == 0)
		goto err1;

	if (mmu_map(map->pgd, phys, reg->addr, size, PG_WRITE))
		goto err2;

	reg->phys = phys;

	/* Zero fill */
	memset(phys_to_virt(phys), 0, reg->size);
	*addr = reg->addr;
	return 0;

 err2:
	page_free(phys, size);
 err1:
	region_free(&map->head, reg);
	return ENOMEM;
}

/*
 * Deallocate memory region for specified address.
 *
 * The "addr" argument points to a memory region previously
 * allocated through a call to vm_allocate() or vm_map(). The
 * number of bytes freed is the number of bytes of the
 * allocated region. If one of the region of previous and next
 * are free, it combines with them, and larger free region is
 * created.
 */
int
vm_free(task_t task, void *addr)
{
	int err;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (task != cur_task() && !task_capable(CAP_MEMORY)) {
		err = EPERM;
		goto out;
	}
	if (!user_area(addr)) {
		err = EFAULT;
		goto out;
	}

	err = do_free(task->map, addr);
 out:
	sched_unlock();
	return err;
}

static int
do_free(vm_map_t map, void *addr)
{
	struct region *reg;

	addr = (void *)PAGE_TRUNC(addr);

	/*
	 * Find the target region.
	 */
	reg = region_find(&map->head, addr, 1);
	if (reg == NULL || reg->addr != addr || (reg->flags & REG_FREE))
		return EINVAL;

	/*
	 * Unmap pages of the region.
	 */
	mmu_map(map->pgd, reg->phys, reg->addr,	reg->size, PG_UNMAP);

	/*
	 * Relinquish use of the page if it is not shared and mapped.
	 */
	if (!(reg->flags & REG_SHARED) && !(reg->flags & REG_MAPPED))
		page_free(reg->phys, reg->size);

	region_free(&map->head, reg);
	return 0;
}

/*
 * Change attribute of specified virtual address.
 *
 * The "addr" argument points to a memory region previously
 * allocated through a call to vm_allocate(). The attribute
 * type can be chosen a combination of VMA_READ, VMA_WRITE.
 * Note: VMA_EXEC is not supported, yet.
 */
int
vm_attribute(task_t task, void *addr, int attr)
{
	int err;

	sched_lock();
	if (attr == 0 || attr & ~(VMA_READ | VMA_WRITE)) {
		err = EINVAL;
		goto out;
	}
	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (task != cur_task() && !task_capable(CAP_MEMORY)) {
		err = EPERM;
		goto out;
	}
	if (!user_area(addr)) {
		err = EFAULT;
		goto out;
	}

	err = do_attribute(task->map, addr, attr);
 out:
	sched_unlock();
	return err;
}

static int
do_attribute(vm_map_t map, void *addr, int attr)
{
	struct region *reg;
	int new_flags = 0;
	void *old_addr, *new_addr = NULL;
	int map_type;

	addr = (void *)PAGE_TRUNC(addr);

	/*
	 * Find the target region.
	 */
	reg = region_find(&map->head, addr, 1);
	if (reg == NULL || reg->addr != addr || (reg->flags & REG_FREE)) {
		return EINVAL;	/* not allocated */
	}
	/*
	 * The attribute of the mapped region can not be changed.
	 */
	if (reg->flags & REG_MAPPED)
		return EINVAL;

	/*
	 * Check new and old flag.
	 */
	if (reg->flags & REG_WRITE) {
		if (!(attr & VMA_WRITE))
			new_flags = REG_READ;
	} else {
		if (attr & VMA_WRITE)
			new_flags = REG_READ | REG_WRITE;
	}
	if (new_flags == 0)
		return 0;	/* same attribute */

	map_type = (new_flags & REG_WRITE) ? PG_WRITE : PG_READ;

	/*
	 * If it is shared region, duplicate it.
	 */
	if (reg->flags & REG_SHARED) {

		old_addr = reg->phys;

		/* Allocate new physical page. */
		if ((new_addr = page_alloc(reg->size)) == 0)
			return ENOMEM;

		/* Copy source page */
		memcpy(phys_to_virt(new_addr), phys_to_virt(old_addr),
		       reg->size);

		/* Map new region */
		if (mmu_map(map->pgd, new_addr, reg->addr, reg->size,
			    map_type)) {
			page_free(new_addr, reg->size);
			return ENOMEM;
		}
		reg->phys = new_addr;

		/* Unlink from shared list */
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
		reg->sh_next = reg->sh_prev = reg;
	} else {
		if (mmu_map(map->pgd, reg->phys, reg->addr, reg->size,
			    map_type))
			return ENOMEM;
	}
	reg->flags = new_flags;
	return 0;
}

/**
 * vm_map - map another task's memory to current task.
 *
 * Note: This routine does not support mapping to the specific address.
 */
int
vm_map(task_t target, void *addr, size_t size, void **alloc)
{
	int err;

	sched_lock();
	if (!task_valid(target)) {
		err = ESRCH;
		goto out;
	}
	if (target == cur_task()) {
		err = EINVAL;
		goto out;
	}
	if (!task_capable(CAP_MEMORY)) {
		err = EPERM;
		goto out;
	}
	if (!user_area(addr)) {
		err = EFAULT;
		goto out;
	}
	err = do_map(target->map, addr, size, alloc);
 out:
	sched_unlock();
	return err;
}

static int
do_map(vm_map_t map, void *addr, size_t size, void **alloc)
{
	vm_map_t curmap;
	char *start, *end, *phys;
	size_t offset;
	struct region *reg, *cur, *tgt;
	task_t self;
	int map_type;
	void *tmp;

	if (size == 0)
		return EINVAL;

	/* check fault */
	tmp = NULL;
	if (umem_copyout(&tmp, alloc, sizeof(tmp)))
		return EFAULT;

	start = (char *)PAGE_TRUNC(addr);
	end = (char *)PAGE_ALIGN((char *)addr + size);
	size = (size_t)(end - start);
	offset = (size_t)((char *)addr - start);

	/*
	 * Find the region that includes target address
	 */
	reg = region_find(&map->head, start, size);
	if (reg == NULL || (reg->flags & REG_FREE))
		return EINVAL;	/* not allocated */
	tgt = reg;

	/*
	 * Find the free region in current task
	 */
	self = cur_task();
	curmap = self->map;
	if ((reg = region_alloc(&curmap->head, size)) == NULL)
		return ENOMEM;
	cur = reg;

	/*
	 * Try to map into current memory
	 */
	if (tgt->flags & REG_WRITE)
		map_type = PG_WRITE;
	else
		map_type = PG_READ;

	phys = (char *)tgt->phys + (start - (char *)tgt->addr);
	if (mmu_map(curmap->pgd, phys, cur->addr, size, map_type)) {
		region_free(&curmap->head, reg);
		return ENOMEM;
	}

	cur->flags = tgt->flags | REG_MAPPED;
	cur->phys = phys;

	tmp = (char *)cur->addr + offset;
	umem_copyout(&tmp, alloc, sizeof(tmp));
	return 0;
}

/*
 * Create new virtual memory space.
 * No memory is inherited.
 *
 * Must be called with scheduler locked.
 */
vm_map_t
vm_create(void)
{
	struct vm_map *map;

	/* Allocate new map structure */
	if ((map = kmem_alloc(sizeof(*map))) == NULL)
		return NULL;

	map->refcnt = 1;

	/* Allocate new page directory */
	if ((map->pgd = mmu_newmap()) == NULL) {
		kmem_free(map);
		return NULL;
	}
	region_init(&map->head);
	return map;
}

/*
 * Terminate specified virtual memory space.
 * This is called when task is terminated.
 */
void
vm_terminate(vm_map_t map)
{
	struct region *reg, *tmp;

	if (--map->refcnt >= 1)
		return;

	sched_lock();
	reg = &map->head;
	do {
		if (reg->flags != REG_FREE) {
			/* Unmap region */
			mmu_map(map->pgd, reg->phys, reg->addr,
				reg->size, PG_UNMAP);

			/* Free region if it is not shared and mapped */
			if (!(reg->flags & REG_SHARED) &&
			    !(reg->flags & REG_MAPPED)) {
				page_free(reg->phys, reg->size);
			}
		}
		tmp = reg;
		reg = reg->next;
		region_delete(&map->head, tmp);
	} while (reg != &map->head);

	mmu_delmap(map->pgd);
	kmem_free(map);
	sched_unlock();
}

/*
 * Duplicate specified virtual memory space.
 * This is called when new task is created.
 *
 * Returns new map id, NULL if it fails.
 *
 * All regions of original memory map are copied to new memory map.
 * If the region is read-only, executable, or shared region, it is
 * no need to copy. These regions are physically shared with the
 * original map.
 */
vm_map_t
vm_fork(vm_map_t org_map)
{
	vm_map_t new_map;

	sched_lock();
	new_map = do_fork(org_map);
	sched_unlock();
	return new_map;
}

static vm_map_t
do_fork(vm_map_t org_map)
{
	vm_map_t new_map;
	struct region *tmp, *src, *dest;
	int map_type;

	if ((new_map = vm_create()) == NULL)
		return NULL;
	/*
	 * Copy all regions
	 */
	tmp = &new_map->head;
	src = &org_map->head;

	/*
	 * Copy top region
	 */
	*tmp = *src;
	tmp->next = tmp->prev = tmp;

	if (src == src->next)	/* Blank memory ? */
		return new_map;

	do {
		ASSERT(src != NULL);
		ASSERT(src->next != NULL);

		if (src == &org_map->head) {
			dest = tmp;
		} else {
			/* Create new region struct */
			dest = kmem_alloc(sizeof(*dest));
			if (dest == NULL)
				return NULL;

			*dest = *src;	/* memcpy */

			dest->prev = tmp;
			dest->next = tmp->next;
			tmp->next->prev = dest;
			tmp->next = dest;
			tmp = dest;
		}
		if (src->flags == REG_FREE) {
			/*
			 * Skip free region
			 */
		} else {
			/* Check if the region can be shared */
			if (!(src->flags & REG_WRITE) &&
			    !(src->flags & REG_MAPPED)) {
				dest->flags |= REG_SHARED;
			}

			if (!(dest->flags & REG_SHARED)) {
				/* Allocate new physical page. */
				dest->phys = page_alloc(src->size);
				if (dest->phys == 0)
					return NULL;

				/* Copy source page */
				memcpy(phys_to_virt(dest->phys),
				       phys_to_virt(src->phys), src->size);
			}
			/* Map the region to virtual address */
			if (dest->flags & REG_WRITE)
				map_type = PG_WRITE;
			else
				map_type = PG_READ;

			if (mmu_map(new_map->pgd, dest->phys, dest->addr,
				    dest->size, map_type))
				return NULL;
		}
		src = src->next;
	} while (src != &org_map->head);

	/*
	 * No error. Now, link all shared regions
	 */
	dest = &new_map->head;
	src = &org_map->head;
	do {
		if (dest->flags & REG_SHARED) {
			src->flags |= REG_SHARED;
			dest->sh_prev = src;
			dest->sh_next = src->sh_next;
			src->sh_next->sh_prev = dest;
			src->sh_next = dest;
		}
		dest = dest->next;
		src = src->next;
	} while (src != &org_map->head);
	return new_map;
}

/*
 * Switch VM mapping.
 *
 * Since a kernel task does not have user mode memory image, we
 * don't have to setup the page directory for it. Thus, an idle
 * thread and interrupt threads can be switched quickly.
 */
void
vm_switch(vm_map_t map)
{

	if (map != &kern_map)
		mmu_switch(map->pgd);
}

/*
 * Increment reference count of VM mapping.
 */
int
vm_reference(vm_map_t map)
{

	map->refcnt++;
	return 0;
}

/*
 * Load task image for boot task.
 * Return 0 on success, -1 on failure.
 */
int
vm_load(vm_map_t map, struct module *mod, void **stack)
{
	char *src;
	void *text, *data;

	DPRINTF(("Loading task: %s\n", mod->name));

	/*
	 * We have to switch VM mapping to touch the virtual
	 * memory space of a target task without page fault.
	 */
	vm_switch(map);

	src = phys_to_virt(mod->phys);
	text = (void *)mod->text;
	data = (void *)mod->data;

	/*
	 * Create text segment
	 */
	if (do_allocate(map, &text, mod->textsz, 0))
		return -1;
	memcpy(text, src, mod->textsz);
	if (do_attribute(map, text, VMA_READ))
		return -1;

	/*
	 * Create data & BSS segment
	 */
	if (mod->datasz + mod->bsssz != 0) {
		if (do_allocate(map, &data, mod->datasz + mod->bsssz, 0))
			return -1;
		src = src + (mod->data - mod->text);
		memcpy(data, src, mod->datasz);
	}
	/*
	 * Create stack
	 */
	*stack = (void *)USTACK_BASE;
	if (do_allocate(map, stack, USTACK_SIZE, 0))
		return -1;

	/* Free original pages */
	page_free((void *)mod->phys, mod->size);
	return 0;
}

/*
 * Translate virtual address of current task to physical address.
 * Returns physical address on success, or NULL if no mapped memory.
 */
void *
vm_translate(void *addr, size_t size)
{
	task_t self = cur_task();

	return mmu_extract(self->map->pgd, addr, size);
}

/*
 * Initialize region
 */
static void
region_init(struct region *reg)
{

	reg->next = reg->prev = reg;
	reg->sh_next = reg->sh_prev = reg;
	reg->addr = (void *)PAGE_SIZE;
	reg->phys = 0;
	reg->size = UMEM_MAX - PAGE_SIZE;
	reg->flags = REG_FREE;
}

/*
 * Create new free region after the specified region.
 * Returns region on success, or NULL on failure.
 */
static struct region *
region_create(struct region *prev, void *addr, size_t size)
{
	struct region *reg;

	if ((reg = kmem_alloc(sizeof(*reg))) == NULL)
		return NULL;

	reg->addr = addr;
	reg->size = size;
	reg->phys = 0;
	reg->flags = REG_FREE;
	reg->sh_next = reg->sh_prev = reg;

	reg->next = prev->next;
	reg->prev = prev;
	prev->next->prev = reg;
	prev->next = reg;
	return reg;
}

/*
 * Delete specified region
 */
static void
region_delete(struct region *head, struct region *reg)
{

	/* If it is shared region, unlink from shared list */
	if (reg->flags & REG_SHARED) {
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
	}
	if (head != reg)
		kmem_free(reg);
}

/*
 * Find the region at the specified area.
 */
static struct region *
region_find(struct region *head, void *addr, size_t size)
{
	struct region *reg;

	reg = head;
	do {
		if (reg->addr <= addr &&
		    (char *)reg->addr + reg->size >= (char *)addr + size) {
			return reg;
		}
		reg = reg->next;
	} while (reg != head);
	return NULL;
}

/*
 * Allocate free region for specified size.
 */
static struct region *
region_alloc(struct region *head, size_t size)
{
	struct region *reg;

	reg = head;
	do {
		if ((reg->flags & REG_FREE) && reg->size >= size) {
			if (reg->size != size) {
				/* Split this region and return its head */
				if (region_create(reg,
						  (char *)reg->addr + size,
						  reg->size - size) == NULL)
					return NULL;
			}
			reg->size = size;
			return reg;
		}
		reg = reg->next;
	} while (reg != head);
	return NULL;
}

/*
 * Delete specified free region
 */
static void
region_free(struct region *head, struct region *reg)
{
	struct region *prev, *next;

	ASSERT(reg->flags != REG_FREE);

	reg->flags = REG_FREE;

	/* If it is shared region, unlink from shared list */
	if (reg->flags & REG_SHARED) {
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
	}

	/* If next region is free, merge with it. */
	next = reg->next;
	if (next != head && (next->flags & REG_FREE)) {
		reg->next = next->next;
		next->next->prev = reg;
		reg->size += next->size;
		kmem_free(next);
	}

	/* If previous region is free, merge with it. */
	prev = reg->prev;
	if (reg != head && (prev->flags & REG_FREE)) {
		prev->next = reg->next;
		reg->next->prev = prev;
		prev->size += reg->size;
		kmem_free(reg);
	}
}

/*
 * Sprit region for the specified address/size.
 */
static struct region *
region_split(struct region *head, struct region *reg, void *addr,
	     size_t size)
{
	struct region *prev, *next;
	size_t diff;

	/*
	 * Check previous region to split region.
	 */
	prev = NULL;
	if (reg->addr != addr) {
		prev = reg;
		diff = (size_t)((char *)addr - (char *)reg->addr);
		reg = region_create(prev, addr, prev->size - diff);
		if (reg == NULL)
			return NULL;
		prev->size = diff;
	}

	/*
	 * Check next region to split region.
	 */
	if (reg->size != size) {
		next = region_create(reg, (char *)reg->addr + size,
				     reg->size - size);
		if (next == NULL) {
			if (prev) {
				/* Undo previous region_create() */
				region_free(head, reg);
			}
			return NULL;
		}
		reg->size = size;
	}
	reg->flags = 0;
	return reg;
}

#ifdef DEBUG
static void
vm_dump_one(task_t task)
{
	vm_map_t map;
	struct region *reg;
	char flags[6];
	size_t total = 0;

	printf("task=%x map=%x name=%s\n", task, task->map,
	       task->name != NULL ? task->name : "no name");
	printf(" region   virtual  physical size     flags\n");
	printf(" -------- -------- -------- -------- -----\n");

	map = task->map;
	reg = &map->head;
	do {
		if (reg->flags != REG_FREE) {
			strlcpy(flags, "-----", 6);
			if (reg->flags & REG_READ)
				flags[0] = 'R';
			if (reg->flags & REG_WRITE)
				flags[1] = 'W';
			if (reg->flags & REG_EXEC)
				flags[2] = 'E';
			if (reg->flags & REG_SHARED)
				flags[3] = 'S';
			if (reg->flags & REG_MAPPED)
				flags[4] = 'M';

			printf(" %08x %08x %08x %8x %s\n", reg,
			       reg->addr, reg->phys, reg->size, flags);
			total += reg->size;
		}
		reg = reg->next;
	} while (reg != &map->head);	/* Process all regions */
	printf(" *total=%dK bytes\n\n", total / 1024);
}

void
vm_dump(void)
{
	list_t n;
	task_t task;

	printf("\nVM dump:\n");
	n = list_first(&kern_task.link);
	while (n != &kern_task.link) {
		task = list_entry(n, struct task, link);
		vm_dump_one(task);
		n = list_next(n);
	}
}
#endif

void
vm_init(void)
{
	pgd_t pgd;

	/*
	 * Setup vm mapping for kernel task.
	 */
	pgd = mmu_newmap();
	ASSERT(pgd != NULL);
	kern_map.pgd = pgd;
	mmu_switch(pgd);
	region_init(&kern_map.head);
	kern_task.map = &kern_map;
}
