#pragma once

#include <types.h>

struct rusage;
struct task;

void	     proc_exit(struct task *, int status, int signal);
