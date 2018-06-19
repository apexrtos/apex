#ifndef clone_h
#define clone_h

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Syscalls
 */
int sc_clone(unsigned long, void *, void *, unsigned long, void *);
int sc_fork(void);
int sc_vfork(void);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* !clone_h */
