/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * vm_nommu.c - virtual memory alloctor for no MMU systems
 */

/*
 * When the platform does not support memory management unit (MMU)
 * all virtual memories are mapped to the physical memory. So, the
 * memory space is shared among all tasks and kernel.
 *
 * Important: The lists of regions are not sorted by address.
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
static void region_free(struct region *, struct region *);
static void region_init(struct region *);
static int do_allocate(vm_map_t, void **, size_t, int);
static int do_free(vm_map_t, void *);
static int do_attribute(vm_map_t, void *, int);
static int do_map(vm_map_t, void *, size_t, void **);


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
	if (umem_copyin(addr, &uaddr, sizeof(*addr))) {
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
	char *start, *end;

	if (size == 0)
		return EINVAL;

	/*
	 * Allocate region, and reserve pages for it.
	 */
	if (anywhere) {
		size = (size_t)PAGE_ALIGN(size);
		if ((start = page_alloc(size)) == 0)
			return ENOMEM;
	} else {
		start = (char *)PAGE_TRUNC(*addr);
		end = (char *)PAGE_ALIGN(start + size);
		size = (size_t)(end - start);

		if (page_reserve(start, size))
			return EINVAL;
	}
	reg = region_create(&map->head, start, size);
	if (reg == NULL) {
     		page_free(start, size);
		return ENOMEM;
	}
	reg->flags = REG_READ | REG_WRITE;

	/* Zero fill */
	memset(start, 0, size);
	*addr = reg->addr;
	return 0;
}

/*
 * Deallocate memory region for specified address.
 *
 * The "addr" argument points to a memory region previously
 * allocated through a call to vm_allocate() or vm_map(). The
 * number of bytes freed is the number of bytes of the
 * allocated region.  If one of the region of previous and
 * next are free, it combines with them, and larger free
 * region is created.
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
		return EINVAL;	/* not allocated */

	/*
	 * Free pages if it is not shared and mapped.
	 */
	if (!(reg->flags & REG_SHARED) && !(reg->flags & REG_MAPPED))
		page_free(reg->addr, reg->size);

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

	addr = (void *)PAGE_TRUNC(addr);

	/*
	 * Find the target region.
	 */
	reg = region_find(&map->head, addr, 1);
	if (reg == NULL || reg->addr != addr || (reg->flags & REG_FREE)) {
		return EINVAL;	/* not allocated */
	}
	/*
	 * The attribute of the mapped or shared region can not be changed.
	 */
	if ((reg->flags & REG_MAPPED) || (reg->flags & REG_SHARED))
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
	reg->flags = new_flags;
	return 0;
}

/**
 * vm_map - map another task's memory to current task.
 *
 * Note: This routine does not support mapping to the specific
 * address.
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
	task_t self;
	char *start, *end;
	struct region *reg, *tgt;
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

	/*
	 * Find the region that includes target address
	 */
	reg = region_find(&map->head, start, size);
	if (reg == NULL || (reg->flags & REG_FREE))
		return EINVAL;	/* not allocated */
	tgt = reg;

	/*
	 * Create new region to map
	 */
	self = cur_task();
	curmap = self->map;
	reg = region_create(&curmap->head, start, size);
	if (reg == NULL)
		return ENOMEM;
	reg->flags = tgt->flags | REG_MAPPED;

	umem_copyout(&addr, alloc, sizeof(addr));
	return 0;
}

/*
 * Create new virtual memory space.
 * No memory is inherited.
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
			/* Free region if it is not shared and mapped */
			if (!(reg->flags & REG_SHARED) &&
			    !(reg->flags & REG_MAPPED)) {
				page_free(reg->addr, reg->size);
			}
		}
		tmp = reg;
		reg = reg->next;
		region_delete(&map->head, tmp);
	} while (reg != &map->head);

	kmem_free(map);
	sched_unlock();
}

/*
 * Duplicate specified virtual memory space.
 */
vm_map_t
vm_fork(vm_map_t org_map)
{
	/*
	 * This function is not supported with no MMU system.
	 */
	return NULL;
}

/*
 * Switch VM mapping.
 */
void
vm_switch(vm_map_t map)
{
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
 * Translate virtual address of current task to physical address.
 * Returns physical address on success, or NULL if no mapped memory.
 */
void *
vm_translate(void *addr, size_t size)
{

	return addr;
}

/*
 * Reserve specific area for boot tasks.
 */
static int
do_reserve(vm_map_t map, void **addr, size_t size)
{
	struct region *reg;
	char *start, *end;

	if (size == 0)
		return EINVAL;

	start = (char *)PAGE_TRUNC(*addr);
	end = (char *)PAGE_ALIGN(start + size);
	size = (size_t)(end - start);

	reg = region_create(&map->head, start, size);
	if (reg == NULL)
		return ENOMEM;
	reg->flags = REG_READ | REG_WRITE;
	*addr = reg->addr;
	return 0;
}

/*
 * Setup task image for boot task. (NOMMU version)
 * Return 0 on success, -1 on failure.
 *
 * Note: We assume that the task images are already copied to
 * the proper address by a boot loader.
 */
int
vm_load(vm_map_t map, struct module *mod, void **stack)
{
	void *base;
	size_t size;

	DPRINTF(("Loading task:\'%s\'\n", mod->name));

	/*
	 * Reserve text & data area
	 */
	base = (void *)mod->text;
	size = mod->textsz + mod->datasz + mod->bsssz;
	if (do_reserve(map, &base, size))
		return -1;
	if (mod->bsssz != 0)
		memset((void *)(mod->data + mod->datasz), 0, mod->bsssz);

	/*
	 * Create stack
	 */
	if (do_allocate(map, stack, USTACK_SIZE, 1))
		return -1;
	return 0;
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

	/*
	 * If it is shared region, unlink from shared list.
	 */
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
 * Free specified region
 */
static void
region_free(struct region *head, struct region *reg)
{
	ASSERT(reg->flags != REG_FREE);

	/*
	 * If it is shared region, unlink from shared list.
	 */
	if (reg->flags & REG_SHARED) {
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
	}
	reg->prev->next = reg->next;
	reg->next->prev = reg->prev;
	kmem_free(reg);
}

/*
 * Initialize region
 */
static void
region_init(struct region *reg)
{

	reg->next = reg->prev = reg;
	reg->sh_next = reg->sh_prev = reg;
	reg->addr = NULL;
	reg->size = 0;
	reg->flags = REG_FREE;
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
	       task->name ? task->name : "no name");
	printf(" region   virtual  size     flags\n");
	printf(" -------- -------- -------- -----\n");

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

			printf(" %08x %08x %08x %s\n", reg,
			       reg->addr, reg->size, flags);
			if ((reg->flags & REG_MAPPED) == 0)
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

	region_init(&kern_map.head);
	kern_task.map = &kern_map;
}
