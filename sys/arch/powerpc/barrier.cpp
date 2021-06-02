#include <sys/include/arch/barrier.h>

/*
 * Complete all memory accesses before starting next memory access
 */
inline void
memory_barrier()
{
	asm volatile("sync" ::: "memory");
}

/*
 * Complete all memory reads before starting next memory access
 */
inline void
read_memory_barrier()
{
	asm volatile("sync" ::: "memory");
}

/*
 * Complete all memory writes before starting next memory access
 */
inline void
write_memory_barrier()
{
	asm volatile("sync" ::: "memory");
}

