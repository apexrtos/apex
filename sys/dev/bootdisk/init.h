#pragma once

/*
 * /dev/bootdiskX
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bootargs;

void bootdisk_init(struct bootargs *);

#ifdef __cplusplus
} /* extern "C" */
#endif
