#pragma once

/*
 * Generic interface to clock drivers
 */

#ifdef __cplusplus
extern "C" {
#endif

unsigned long clock_ns_since_tick();

#ifdef __cplusplus
} /* extern "C" */
#endif
