#pragma once

#include <span>
#include <sys/uio.h>
#include <types.h>

struct dirent;

/*
 * dirbuf_add - helper for adding entries to a dirent buffer
 */
int dirbuf_add(dirent **, size_t *remain, ino_t, off_t,
	       unsigned char type, const char *name);

/*
 * for_each_iov - C++ version of for_each_iov
 */
template<typename Fn>
ssize_t
for_each_iov(const iovec *iov, size_t count, off_t offset, Fn &&fn)
{
	ssize_t total = 0;
	ssize_t res = 0;
	while (count--) {
		const std::span<std::byte> buf{
			static_cast<std::byte *>(iov->iov_base),
			iov->iov_len
		};
		if ((res = fn(buf, offset + total)) < 0)
			break;
		total += res;
		if ((size_t)res != iov->iov_len)
			return total;
		++iov;
	}
	return total > 0 ? total : res;
}
