#pragma once

/*
 * sig - signal handling, generation and delivery
 */

#include <stdbool.h>
#include <types.h>

struct k_sigaction;
struct task;
struct thread;

/*
 * Kernel interface
 */
int	    sig_task(struct task *, int);
void	    sig_thread(struct thread *, int);
bool	    sig_unblocked_pending(struct thread *);
k_sigset_t  sig_block_all();
void	    sig_restore(const k_sigset_t *);
void	    sig_exec(struct task *);
void	    sig_wait();
extern "C" int sig_deliver(int);

/*
 * Syscalls
 */
int	sc_rt_sigaction(int signum, const struct k_sigaction *uact,
			struct k_sigaction *oldact, size_t size);
int	sc_rt_sigprocmask(int how, const k_sigset_t *set, k_sigset_t *oldset,
			  size_t size);
int	sc_rt_sigreturn();
int	sc_sigreturn();
