#ifndef arm_v7m_errno_h
#define arm_v7m_errno_h

#ifndef __ASSEMBLY__
#include_next <errno.h>
#endif

/*
 * sig_deliver will return ETHREAD_EXIT when a thread is exiting
 */
#define ETHREAD_EXIT 255

/*
 * sigreturn will return ERESTARTSYS if a system call was interrupted
 * by a signal with SA_RESTART set.
 */
#define ERESTARTSYS 256

#endif
