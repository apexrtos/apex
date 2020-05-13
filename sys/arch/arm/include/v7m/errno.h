#ifndef arm_v7m_errno_h
#define arm_v7m_errno_h

#ifndef __ASSEMBLY__
#include_next <errno.h>
#endif

/*
 * sigreturn will return EINTERRUPT_RETURN after an asynchronous
 * signal has been delivered.
 */
#define EINTERRUPT_RETURN 255

/*
 * sig_deliver will return ETHREAD_EXIT when a thread is exiting
 */
#define ETHREAD_EXIT EINTERRUPT_RETURN

/*
 * sigreturn will return ERESTARTSYS if a system call was interrupted
 * by a signal with SA_RESTART set.
 */
#define ERESTARTSYS 256

#endif
