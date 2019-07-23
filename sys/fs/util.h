#ifndef fs_util_h
#define fs_util_h

#include <types.h>

struct dirent;
struct file;
struct iovec;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * dirbuf_add - helper for adding entries to a dirent buffer
 */
int dirbuf_add(struct dirent **, size_t *remain, ino_t, off_t,
	       unsigned char type, const char *name);

/*
 * for_each_iov - walk iov, calling function fn for each entry and
 * accumulating results. Terminate on short result.
 */
ssize_t	for_each_iov(struct file *, const struct iovec *, size_t, off_t,
		     ssize_t (*fn)(struct file *, void *, size_t, off_t));

#if defined(__cplusplus)
}
#endif

#endif /* fs_util_h */
