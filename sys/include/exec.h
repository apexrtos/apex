#pragma once

struct task;
struct thread;

struct thread* exec_into(struct task *, const char *path,
			 const char *const argv[], const char *const envp[]);
