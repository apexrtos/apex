#include <fs/util.h>

#include <dirent.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

int
dirbuf_add(struct dirent **buf, size_t *remain, ino_t ino, off_t off,
    unsigned char type, const char *name)
{
	const size_t align = alignof(struct dirent);
	const size_t dirent_noname = offsetof(struct dirent, d_name);
	const size_t name_max = *remain - dirent_noname;
	if ((ssize_t)name_max < 2)
		return -1;
	const size_t name_len = strlcpy((*buf)->d_name, name, name_max);
	if (name_len >= name_max)
		return -1;
	const size_t reclen = (dirent_noname + name_len + align) & -align;
	(*buf)->d_ino = ino;
	(*buf)->d_off = off;
	(*buf)->d_reclen = reclen;
	(*buf)->d_type = type;
	*remain -= reclen;
	*buf = (struct dirent *)((char *)*buf + reclen);
	return 0;
}
