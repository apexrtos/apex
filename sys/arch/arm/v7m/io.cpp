/*
 * NOTE: this file is also used by boot loader
 */
#include <sys/include/arch/barrier.h>
#include <sys/include/arch/mmio.h>

/*
 * Complete all memory accesses before starting next memory access
 */
void
memory_barrier()
{
	asm("dmb" ::: "memory");
}

/*
 * Complete all memory reads before starting next memory access
 */
void
read_memory_barrier()
{
	asm("dmb" ::: "memory");
}

/*
 * Complete all memory writes before starting next memory access
 */
void
write_memory_barrier()
{
	asm("dmb" ::: "memory");
}

/*
 * Read uint8_t from memory location p
 */
uint8_t
mmio_read8(const void *p)
{
	return *(const volatile uint8_t *)p;
}

/*
 * Read uint16_t from memory location p
 */
uint16_t
mmio_read16(const void *p)
{
	return *(const volatile uint16_t *)p;
}

/*
 * Read uint32_t from memory location p
 */
uint32_t
mmio_read32(const void *p)
{
	return *(const volatile uint32_t *)p;
}

/*
 * Write uint8_t to memory location p
 */
void
mmio_write8(void *p, uint8_t v)
{
	*(volatile uint8_t *)p = v;
}

/*
 * Write uint16_t to memory location p
 */
void
mmio_write16(void *p, uint16_t v)
{
	*(volatile uint16_t *)p = v;
}

/*
 * Write uint32_t to memory location p
 */
void
mmio_write32(void *p, uint32_t v)
{
	*(volatile uint32_t *)p = v;
}
