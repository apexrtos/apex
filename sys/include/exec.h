#pragma once

#include <lib/expect.h>

struct task;
struct thread;

expect<thread *>
exec_into(task *, const char *path, const char *const argv[],
	  const char *const envp[]);
