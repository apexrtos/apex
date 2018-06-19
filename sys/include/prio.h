#ifndef prio_h
#define prio_h

struct thread;

int	prio_inherit(struct thread *);
void	prio_reset(struct thread *);

#endif /* !prio_h */
