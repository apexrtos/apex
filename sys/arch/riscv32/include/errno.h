#pragma once

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
 *
 * In practise this will never happen on this architecture: sch_testexit()
 * will immediately reschedule away from a thread which is exiting.
 */
#define ETHREAD_EXIT EINTERRUPT_RETURN

/*
 * sigreturn will return ERESTARTSYS if a system call was interrupted
 * by a signal with SA_RESTART set.
 */
#define ERESTARTSYS 256
