#pragma once

#include <sys/types.h>

struct file;

int	pipe_open(file *, int, mode_t);
int	pipe_close(file *);
ssize_t	pipe_read(file *, void *, size_t, off_t);
ssize_t	pipe_write(file *, void *, size_t, off_t);
