#pragma once

/*
 * vm.h - virtual memory
 */

struct as;
struct iovec;

void vm_init();
void vm_dump();
void vm_init_brk(as *, void *);
ssize_t vm_readv(as *, const iovec *, size_t, const iovec *, size_t);
ssize_t vm_writev(as *, const iovec *, size_t, const iovec *, size_t);
ssize_t vm_read(as *, void *, const void *, size_t);
ssize_t vm_write(as *, const void *, void *, size_t);
ssize_t vm_copy(as *, void *, const void *, size_t);
