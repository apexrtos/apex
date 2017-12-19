/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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

#ifndef _QUEUE_H
#define _QUEUE_H

#include <sys/cdefs.h>

struct queue {
	struct queue *next;
	struct queue *prev;
};
typedef struct queue *queue_t;

#define queue_init(head)	((head)->next = (head)->prev = (head))
#define queue_empty(head)	((head)->next == (head))
#define queue_next(q)		((q)->next)
#define queue_prev(q)		((q)->prev)
#define queue_first(head)	((head)->next)
#define queue_last(head)	((head)->prev)
#define queue_end(head,q)	((q) == (head))

/* Get the struct for this entry */
#define queue_entry(q, type, member) \
    ((type *)((char *)(q) - (unsigned long)(&((type *)0)->member)))

__BEGIN_DECLS
void	 enqueue(queue_t, queue_t);
queue_t	 dequeue(queue_t);
void	 queue_insert(queue_t, queue_t);
void	 queue_remove(queue_t);
__END_DECLS

#endif /* !_QUEUE_H */
