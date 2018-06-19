#ifndef proc_h
#define proc_h

#include <types.h>

struct rusage;
struct task;

/*
 * Kernel interface
 */
void	     proc_exit(struct task *, int status);

/*
 * Syscalls
 */
pid_t	     sc_wait4(pid_t, int *ustatus, int options, struct rusage *);
int	     sc_tkill(int, int);
int	     sc_tgkill(pid_t, int, int);

#endif /* !proc_h */
