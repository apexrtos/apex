#ifndef fs_util_h
#define fs_util_h

#include <types.h>

struct dirent;
struct file;
struct iovec;

/*
 * dirbuf_add - helper for adding entries to a dirent buffer
 */
int dirbuf_add(struct dirent **, size_t *remain, ino_t, off_t,
	       unsigned char type, const char *name);

/*
 * for_each_iov - walk iov, calling function fn for each entry and
 * accumulating results. Terminate on short result.
 */
ssize_t	for_each_iov(struct file *, const struct iovec *, size_t,
		     ssize_t (*fn)(struct file *, void *, size_t));

#endif /* fs_util_h */
