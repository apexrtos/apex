#pragma once

#include <types.h>

struct task;

void	     proc_exit(struct task *, int status, int signal);
