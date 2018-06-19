#ifndef exec_h
#define exec_h

struct task;

#if defined(__cplusplus)
extern "C" {
#endif

int exec_into(struct task *, const char *path, const char *const argv[],
	      const char *const envp[]);

/*
 * Syscalls
 */
int sc_execve(const char *path, const char *const argv[],
	      const char *const envp[]);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* !exec_h */
