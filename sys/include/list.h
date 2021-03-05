/*
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

#pragma once

struct list {
	struct list *next;
	struct list *prev;
};

#define list_init(head)		((head)->next = (head)->prev = (head))
#define list_next(node)		((node)->next)
#define list_prev(node)		((node)->prev)
#define list_empty(head)	((head)->next == (head))
#define list_first(head)	((head)->next)
#define list_last(head)		((head)->prev)
#define list_end(head, node)	((node) == (head))
#define list_only_entry(node)	((node)->next == (node)->prev)

/*
 * Get the struct for this entry
 */
#define list_entry(p, type, member) \
    ((type *)((char *)(p) - (unsigned long)(&((type *)0)->member)))

#define LIST_INIT(head) { &(head), &(head) }

/*
 * Insert new node after specified node
 */
static inline list *
list_insert(list *prev, list *node)
{
	/* order is important when a node is asynchronously inserted */
	node->next = prev->next;
	node->prev = prev;
	prev->next->prev = node;
	prev->next = node;
	return node;
}

/*
 * Remove specified node from list
 */
static inline list *
list_remove(list *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
	return node;
}

/* iterate over a list of a given type */
/* NOTE: typeof() is a gcc extension */
#define list_for_each_entry(ptr, head, member)				   \
	for (__builtin_prefetch((head)->next),                             \
		     ptr = list_entry((head)->next, typeof(*ptr), member), \
		     __builtin_prefetch(ptr->member.next);                 \
	     &ptr->member != (head);					   \
	     ptr = list_entry(ptr->member.next, typeof(*ptr), member),	   \
		     __builtin_prefetch(ptr->member.next))

/*
 * iterate over list of given type - safe against removal of list entry
 * ptr:    the type * to use as a loop counter.
 * tmp:    temporary storage, same as ptr
 * head:   the head for your list.
 * member: the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(ptr, tmp, head, member)		\
	for (ptr = list_entry((head)->next, typeof(*ptr), member),	\
		     tmp = list_entry(ptr->member.next, typeof(*ptr), member); \
	     &ptr->member != (head);					\
	     ptr = tmp,							\
		     tmp = list_entry(tmp->member.next, typeof(*tmp), member))
