#pragma once

struct task;
struct thread;

#if defined(__cplusplus)
extern "C" {
#endif

struct thread* exec_into(struct task *, const char *path,
			 const char *const argv[], const char *const envp[]);

/*
 * Syscalls
 */
int sc_execve(const char *path, const char *const argv[],
	      const char *const envp[]);

#if defined(__cplusplus)
} /* extern "C" */
#endif
