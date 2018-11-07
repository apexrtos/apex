#include <sys/include/arch.h>

/*
 * Complete all memory accesses before starting next memory access
 */
void
memory_barrier(void)
{
	asm volatile("dmb" ::: "memory");
}

/*
 * Complete all memory reads before starting next memory access
 */
void
read_memory_barrier(void)
{
	asm volatile("dmb" ::: "memory");
}

/*
 * Complete all memory writes before starting next memory access
 */
void
write_memory_barrier(void)
{
	asm volatile("dmb" ::: "memory");
}

/*
 * Read uint8_t from memory location p
 */
inline uint8_t
io_read8(const uint8_t *p)
{
	uint8_t v;
	asm volatile("ldrb %0, %1" : "=r"(v) : "m"(*p));
	return v;
}

/*
 * Read uint16_t from memory location p
 */
inline uint16_t
io_read16(const uint16_t *p)
{
	uint16_t v;
	asm volatile("ldrh %0, %1" : "=r"(v) : "m"(*p));
	return v;
}

/*
 * Read uint32_t from memory location p
 */
inline uint32_t
io_read32(const uint32_t *p)
{
	uint32_t v;
	asm volatile("ldr %0, %1" : "=r"(v) : "m"(*p));
	return v;
}

/*
 * Write uint8_t to memory location p
 */
inline void
io_write8(uint8_t *p, uint8_t v)
{
	asm volatile("strb %1, %0" : "=m"(*p) : "r"(v));
}

/*
 * Write uint16_t to memory location p
 */
inline void
io_write16(uint16_t *p, uint16_t v)
{
	asm volatile("strh %1, %0" : "=m"(*p) : "r"(v));
}

/*
 * Write uint32_t to memory location p
 */
inline void
io_write32(uint32_t *p, uint32_t v)
{
	asm volatile("str %1, %0" : "=m"(*p) : "r"(v));
}
