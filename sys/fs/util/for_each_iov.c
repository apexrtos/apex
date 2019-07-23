#include <fs/util.h>

#include <sys/uio.h>

ssize_t
for_each_iov(struct file *fp, const struct iovec *iov, size_t count,
    off_t offset, ssize_t (*fn)(struct file *, void *, size_t, off_t))
{
	ssize_t total = 0;
	ssize_t res = 0;
	while (count--) {
		if ((res = fn(fp, iov->iov_base, iov->iov_len,
		    offset + total)) < 0)
			break;
		total += res;
		if ((size_t)res != iov->iov_len)
			return total;
		++iov;
	}
	return total > 0 ? total : res;
}

