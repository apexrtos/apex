/*
 * NOTE: this file is also used by boot loader
 *
 * https://www.ibm.com/developerworks/systems/articles/powerpc.html
 */
#include <sys/include/arch/mmio.h>

/*
 * Read uint8_t from memory location p
 */
uint8_t
mmio_read8(const void *p)
{
	uint8_t v;
	asm volatile("lbz%U1%X1 %0, %1" : "=r"(v) : "m<>"(*(const uint8_t *)p));
	asm volatile("isync");
	return v;
}

/*
 * Read uint16_t from memory location p
 */
uint16_t
mmio_read16(const void *p)
{
	uint16_t v;
	asm volatile("lhz%U1%X1 %0, %1" : "=r"(v) : "m<>"(*(const uint16_t *)p));
	asm volatile("isync");
	return v;
}

/*
 * Read uint32_t from memory location p
 */
uint32_t
mmio_read32(const void *p)
{
	uint32_t v;
	asm volatile("lwz%U1%X1 %0, %1" : "=r"(v) : "m<>"(*(const uint32_t *)p));
	asm volatile("isync");
	return v;
}

/*
 * Write uint8_t to memory location p
 */
void
mmio_write8(void *p, uint8_t v)
{
	asm volatile("stb%U0%X0 %1, %0" : "=m<>"(*(uint8_t *)p) : "r"(v));
}

/*
 * Write uint16_t to memory location p
 */
void
mmio_write16(void *p, uint16_t v)
{
	asm volatile("sth%U0%X0 %1, %0" : "=m<>"(*(uint16_t *)p) : "r"(v));
}

/*
 * Write uint32_t to memory location p
 */
void
mmio_write32(void *p, uint32_t v)
{
	asm volatile("stw%U0%X0 %1, %0" : "=m<>"(*(uint32_t *)p) : "r"(v));
}
