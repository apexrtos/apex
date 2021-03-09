#pragma once

struct task;
struct thread;

thread *exec_into(task *, const char *path, const char *const argv[],
		  const char *const envp[]);
