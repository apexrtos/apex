#pragma once

/*
 * sig - signal handling, generation and delivery
 */

#include <types.h>

struct task;
struct thread;

int	    sig_task(struct task *, int);
void	    sig_thread(struct thread *, int);
bool	    sig_unblocked_pending(struct thread *);
k_sigset_t  sig_block_all();
void	    sig_restore(const k_sigset_t *);
void	    sig_exec(struct task *);
void	    sig_wait();
extern "C" int sig_deliver(int);
