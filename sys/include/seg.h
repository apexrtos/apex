#pragma once

/*
 * seg.h - segment management
 */

#include <lib/expect.h>

struct as;
struct seg;
struct vnode;

expect<const seg *> as_find_seg(const as *, const void *);
void *seg_begin(const seg *);
void *seg_end(const seg *);
size_t seg_size(const seg *);
int seg_prot(const seg *);
vnode *seg_vnode(seg *);
