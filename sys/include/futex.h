#pragma once

struct task;

#define FUTEX_WAIT		0x0
#define FUTEX_WAKE		0x1
#define FUTEX_FD		0x2
#define FUTEX_REQUEUE		0x3
#define FUTEX_CMP_REQUEUE	0x4
#define FUTEX_WAKE_OP		0x5
#define FUTEX_LOCK_PI		0x6
#define FUTEX_UNLOCK_PI		0x7
#define FUTEX_TRYLOCK_PI	0x8
#define FUTEX_WAIT_BITSET	0x9
#define FUTEX_PRIVATE		0x080
#define FUTEX_CLOCK_REALTIME	0x100
#define FUTEX_OP_MASK		~(FUTEX_PRIVATE | FUTEX_CLOCK_REALTIME)

#define FUTEX_OWNER_DIED	0x40000000
#define FUTEX_WAITERS           0x80000000

/*
 * futex state stored on task
 */
struct futexes {
	union {
		char storage[12];
		unsigned align;
	};
};

int futex(task *, int *, int, int, void *, int *);
void futexes_init(futexes *);
void futexes_destroy(futexes *);
