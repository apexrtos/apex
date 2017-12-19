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

/*
 * queue.c - generic queue management library
 */
#include <queue.h>

/*
 * Insert element at tail of queue
 */
void
enqueue(queue_t head, queue_t item)
{
	item->next = head;
	item->prev = head->prev;
	item->prev->next = item;
	head->prev = item;
}

/*
 * Remove and return element of head of queue
 */
queue_t
dequeue(queue_t head)
{
	queue_t item;

	if (head->next == head)
		return ((queue_t)0);
	item = head->next;
	item->next->prev = head;
	head->next = item->next;
	return item;
}

/*
 * Insert element after specified element
 */
void
queue_insert(queue_t prev, queue_t item)
{
	item->prev = prev;
	item->next = prev->next;
	prev->next->prev = item;
	prev->next = item;
}

/*
 * Remove specified element from queue
 */
void
queue_remove(queue_t item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
}
