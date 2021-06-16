#pragma once

/*
 * vm.h - virtual memory
 */

#include <lib/expect.h>

struct as;
struct iovec;

void vm_init();
void vm_dump();
void vm_init_brk(as *, void *);
expect_pos vm_readv(as *, const iovec *, size_t, const iovec *, size_t);
expect_pos vm_writev(as *, const iovec *, size_t, const iovec *, size_t);
expect_pos vm_read(as *, void *, const void *, size_t);
expect_pos vm_write(as *, const void *, void *, size_t);
expect_pos vm_copy(as *, void *, const void *, size_t);
