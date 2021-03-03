/*
 * NOTE: this file is also used by boot loader
 */
#include <sys/include/arch.h>

/*
 * Complete all memory accesses before starting next memory access
 */
void
memory_barrier()
{
	asm volatile("dmb" ::: "memory");
}

/*
 * Complete all memory reads before starting next memory access
 */
void
read_memory_barrier()
{
	asm volatile("dmb" ::: "memory");
}

/*
 * Complete all memory writes before starting next memory access
 */
void
write_memory_barrier()
{
	asm volatile("dmb" ::: "memory");
}

/*
 * Read uint8_t from memory location p
 */
uint8_t
mmio_read8(const void *p)
{
	uint8_t v;
	asm volatile("ldrb %0, %1" : "=lh"(v) : "m"(*(const uint8_t *)p));
	return v;
}

/*
 * Read uint16_t from memory location p
 */
uint16_t
mmio_read16(const void *p)
{
	uint16_t v;
	asm volatile("ldrh %0, %1" : "=lh"(v) : "m"(*(const uint16_t *)p));
	return v;
}

/*
 * Read uint32_t from memory location p
 */
uint32_t
mmio_read32(const void *p)
{
	uint32_t v;
	asm volatile("ldr %0, %1" : "=lh"(v) : "m"(*(const uint32_t *)p));
	return v;
}

/*
 * Write uint8_t to memory location p
 */
void
mmio_write8(void *p, uint8_t v)
{
	asm volatile("strb %1, %0" : "=m"(*(uint8_t *)p) : "lh"(v));
}

/*
 * Write uint16_t to memory location p
 */
void
mmio_write16(void *p, uint16_t v)
{
	asm volatile("strh %1, %0" : "=m"(*(uint16_t *)p) : "lh"(v));
}

/*
 * Write uint32_t to memory location p
 */
void
mmio_write32(void *p, uint32_t v)
{
	asm volatile("str %1, %0" : "=m"(*(uint32_t *)p) : "lh"(v));
}
