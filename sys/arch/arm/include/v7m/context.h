#pragma once

/*
 * Context stored in struct thread
 */
struct context {
	void *tls;		    /* userspace tls address */
	void *usp;		    /* user stack pointer */
	void *kstack;		    /* bottom of kernel stack */
	void *ksp;		    /* kernel stack pointer */
	void *vfork_eframe;	    /* vfork saved exception frame */
};
