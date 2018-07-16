/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <arch.h>
#include <assert.h>
#include <console.h>
#include <debug.h>
#include <dev/null/null.h>
#include <dev/zero/zero.h>
#include <device.h>
#include <elf_load.h>
#include <exec.h>
#include <fcntl.h>
#include <fs.h>
#include <irq.h>
#include <kmem.h>
#include <mmap.h>
#include <page.h>
#include <proc.h>
#include <sch.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <task.h>
#include <thread.h>
#include <unistd.h>
#include <version.h>
#include <vm.h>

static void
boot_thread(void *arg);

/*
 * Initialization code.
 *
 * Called from kernel_start()
 * We assume that the following machine state:
 *	- .bss section is filled with 0
 *	- .data section is initialised
 *	- stack is configured
 *	- interrupts are disabled
 *	- minimum page table is set (MMU systems only)
 */
void
kernel_main(void)
{
	sch_lock();
#if defined(CONFIG_EARLY_CONSOLE)
	early_console_init();
#endif
	info("APEX " VERSION_STRING " for " CONFIG_MACHINE_NAME "\n");

	/*
	 * Initialize memory managers.
	 */
	machine_memory_init();
	page_init(&bootinfo, &kern_task);
	kmem_init();

	/*
	 * Do machine-dependent
	 * initialization.
	 */
	machine_init();

	/*
	 * Initialize kernel core.
	 */
	vm_init();
	task_init();
	thread_init();
	sch_init();
	timer_init();

	/*
	 * Enable interrupt and initialise drivers.
	 */
	irq_init();
	clock_init();
	device_init();
	null_init();
	zero_init();
	machine_driver_init();
	fs_init();

	/*
	 * Create boot thread and start scheduler.
	 */
	kthread_create(&boot_thread, NULL, PRI_DEFAULT, "boot", MEM_NORMAL);
	machine_ready();
	sch_unlock();
	thread_idle();
}

static void
mount_fs(void)
{
	/*
	 * Root file system is always RAMFS
	 */
	if (mount(NULL, "/", "ramfs", 0, NULL) < 0)
		panic("failed to create root file system");

	/*
	 * Initialise kernel task file system state
	 */
	fs_kinit();

	/*
	 * Create dev directory
	 */
	if (mkdir("/dev", 0) < 0)
		panic("failed to create /dev directory");

	/*
	 * Create boot directory
	 */
	if (mkdir("/boot", 0) < 0)
		panic("failed to create /boot directory");

	/*
	 * Mount devfs
	 */
	if (mount(NULL, "/dev", "devfs", 0, NULL) < 0)
		panic("failed to mount /dev");

	/*
	 * Mount /boot file system according to config options
	 */
	if (mount(CONFIG_BOOTDEV, "/boot", CONFIG_BOOTFS, 0, NULL) < 0)
		panic("failed to mount /boot");
}

static void
run_init(void)
{
	/*
	 * Split CONFIG_INITCMD into init command and arguments
	 */
	const char *ws = " \t", *argv[8];
	char *sv, c[] = CONFIG_INITCMD;
	size_t i = 0;
	for (const char *s = strtok_r(c, ws, &sv);
	    i < ARRAY_SIZE(argv) && s; s = strtok_r(0, ws, &sv))
		argv[i++] = s;
	if (i == ARRAY_SIZE(argv))
		panic("too many init args");
	argv[i] = 0;

	/*
	 * Create init task
	 */
	struct task *task;
	if (task_create(&kern_task, VM_NEW, &task) < 0)
		panic("task_create");
	fs_fork(task);

	/*
	 * Run init
	 */
	struct thread *th;
	as_modify_begin(task->as);
	if ((th = exec_into(task, argv[0], argv, 0)) > (struct thread*)-4096UL)
		panic("failed to run init");

	/*
	 * Open stdin, stdout, stderr
	 */
	if (openfor(task, AT_FDCWD, "/dev/console", O_RDWR) < 0)
		dbg("failed to open /dev/console\n");
	else {
		if (dup2for(task, 0, 1) < 0)
			panic("dup2for");
		if (dup2for(task, 0, 2) < 0)
			panic("dup2for");
	}

	thread_resume(th);
}

static void
boot_thread(void *arg)
{
	interrupt_enable(); /* kthreads start with interrupts disabled */
	mount_fs();
#if defined(CONFIG_CONSOLE)
	console_init();
#endif
	run_init();
	kthread_terminate(thread_cur());
	sch_exit();
}
