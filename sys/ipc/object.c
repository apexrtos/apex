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
 * object.c - object service
 */

/*
 * General Design:
 *
 * An object represents service, state, or policies etc. To
 * manipulate objects, kernel provide 3 functions: create, delete,
 * lookup. Prex task will create an object to provide its interface
 * to other tasks. The tasks will communicate by sending a message
 * to the object each other. For example, a server task creates some
 * objects and client task will send a request message to it.
 *
 * A substance of object is stored in kernel space, and it is
 * protected from user mode code. Each object data is managed with
 * the hash table by using its name string. Usually, an object has
 * a unique name within a system. Before a task sends a message to
 * the specific object, it must obtain the object ID by looking up
 * the name of the target object.
 *
 * An object can be created without its name. These object can be
 * used as private objects that are used by threads in same task.
 */

#include <kernel.h>
#include <queue.h>
#include <kmem.h>
#include <sched.h>
#include <task.h>
#include <ipc.h>

#define OBJ_MAXBUCKETS	32	/* Size of object hash buckets */

/*
 * Object hash table
 *
 * All objects are hashed by its name string. If an object
 * has no name, it is linked to index zero. The scheduler
 * must be locked when this table is touched.
 */
static struct list obj_table[OBJ_MAXBUCKETS];

/*
 * Calculate the hash index for specified name string.
 * The name can be NULL if the object does not have its
 * name.
 */
static u_int
object_hash(const char *name)
{
	u_int h = 0;

	if (name == NULL)
		return 0;
	while (*name)
		h = ((h << 5) + h) + *name++;
	return h & (OBJ_MAXBUCKETS - 1);
}

/*
 * Helper function to find the object from the specified name.
 * Returns NULL if not found.
 */
static object_t
object_find(const char *name)
{
	list_t head, n;
	object_t obj = NULL;

	head = &obj_table[object_hash(name)];

	for (n = list_first(head); n != head; n = list_next(n)) {
		obj = list_entry(n, struct object, hash_link);
		if (!strncmp(obj->name, name, MAXOBJNAME))
			break;
	}
	if (n == head)
		return NULL;
	return obj;
}

/*
 * Search an object in the object name space. The object
 * name must be null-terminated string. The object ID is
 * returned in obj on success.
 */
int
object_lookup(const char *name, object_t *objp)
{
	object_t obj;
	size_t len;
	char str[MAXOBJNAME];

	if (umem_strnlen(name, MAXOBJNAME, &len))
		return EFAULT;
	if (len == 0 || len >= MAXOBJNAME)
		return ESRCH;
	if (umem_copyin(name, str, len + 1))
		return EFAULT;

	sched_lock();
	obj = object_find(str);
	sched_unlock();

	if (obj == NULL)
		return ENOENT;
	if (umem_copyout(&obj, objp, sizeof(obj)))
		return EFAULT;
	return 0;
}

/*
 * Create a new object.
 *
 * The ID of the new object is stored in pobj on success.
 * The name of the object must be unique in the system.
 * Or, the object can be created without name by setting
 * NULL as name argument. This object can be used as a
 * private object which can be accessed only by threads in
 * same task.
 */
int
object_create(const char *name, object_t *objp)
{
	struct object *obj = 0;
	char str[MAXOBJNAME];
	task_t self;
	size_t len = 0;

	if (name != NULL) {
		if (umem_strnlen(name, MAXOBJNAME, &len))
			return EFAULT;
		if (len >= MAXOBJNAME)
			return ENAMETOOLONG;
		if (umem_copyin(name, str, len + 1))
			return EFAULT;
	}
	str[len] = '\0';
	sched_lock();

	/*
	 * Check user buffer first. This can reduce the error
	 * recovery for the subsequence resource allocations.
	 */
	if (umem_copyout(&obj, objp, sizeof(obj))) {
		sched_unlock();
		return EFAULT;
	}
	if (object_find(str) != NULL) {
		sched_unlock();
		return EEXIST;
	}
	if ((obj = kmem_alloc(sizeof(*obj))) == NULL) {
		sched_unlock();
		return ENOMEM;
	}
	if (name != NULL)
		strlcpy(obj->name, str, len + 1);

	self = cur_task();
	obj->owner = self;
	obj->magic = OBJECT_MAGIC;
	queue_init(&obj->sendq);
	queue_init(&obj->recvq);
	list_insert(&obj_table[object_hash(name)], &obj->hash_link);
	list_insert(&self->objects, &obj->task_link);

	umem_copyout(&obj, objp, sizeof(obj));
	sched_unlock();
	return 0;
}

/*
 * Destroy an object.
 *
 * A thread can delete the object only when the target
 * object is created by the thread of the same task.  All
 * pending messages related to the deleted object are
 * automatically canceled.
 */
int
object_destroy(object_t obj)
{
	int err = 0;

	sched_lock();
	if (!object_valid(obj)) {
		err = EINVAL;
	} else if (obj->owner != cur_task()) {
		err = EACCES;
	} else {
		obj->magic = 0;
		msg_cancel(obj);
		list_remove(&obj->task_link);
		list_remove(&obj->hash_link);
		kmem_free(obj);
	}
	sched_unlock();
	return err;
}

void
object_init(void)
{
	int i;

	for (i = 0; i < OBJ_MAXBUCKETS; i++)
		list_init(&obj_table[i]);
}
