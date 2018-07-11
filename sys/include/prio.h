#ifndef prio_h
#define prio_h

struct thread;

#if defined(__cplusplus)
extern "C" {
#endif

int	prio_inherit(struct thread *);
void	prio_reset(struct thread *);

#if defined(__cplusplus)
}
#endif

#endif /* !prio_h */
