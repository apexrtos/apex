#pragma once

/*
 * Structures required for legacy 32 bit time syscalls
 */

struct timespec32 {
	int32_t tv_sec;
	int32_t tv_nsec;
};

/*
 * kernel itimerval uses native 'long' types except for x32
 */
struct k_itimerval {
#if defined(__ILP32__)
	struct timeval it_interval;
	struct timeval it_value;
#else
	struct {
		long tv_sec;
		long tv_usec;
	} it_interval;
	struct {
		long tv_sec;
		long tv_usec;
	} it_value;
#endif
};
