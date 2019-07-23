#ifndef fs_pipe_h
#define fs_pipe_h

#include <sys/types.h>

struct file;

int	pipe_open(struct file *, int, mode_t);
int	pipe_close(struct file *);
ssize_t	pipe_read(struct file *, void *, size_t, off_t);
ssize_t	pipe_write(struct file *, void *, size_t, off_t);

#endif
