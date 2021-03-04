#pragma once

/*
 * arch/context.h - architecture specific context management
 */

#include <csignal>

struct as;
struct context;
struct k_sigset_t;
struct thread;

void arch_schedule();
void context_init_idle(context *, void *kstack_top);
void context_init_kthread(context *, void *kstack_top,
			  void (*entry)(void *), void *arg);
int context_init_uthread(context *, as *, void *kstack_top,
			 void *ustack_top, void (*entry)(), long rval);
void context_restore_vfork(context *, as *);
bool context_set_signal(context *, const k_sigset_t *, void (*handler)(int),
			void (*restorer)(), int sig, const siginfo_t *, int rval);
void context_set_tls(context *, void *tls);
void context_switch(thread *, thread *);
bool context_restore(context *, k_sigset_t *, int *rval, bool siginfo);
void context_terminate(thread *);
void context_free(context *);
