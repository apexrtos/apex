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
#include <bootargs.h>
#include <debug.h>
#include <dev/null/null.h>
#include <dev/zero/zero.h>
#include <device.h>
#include <exec.h>
#include <fcntl.h>
#include <fs.h>
#include <irq.h>
#include <kmem.h>
#include <sch.h>
#include <string.h>
#include <sys/mount.h>
#include <task.h>
#include <thread.h>
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
extern "C" void
kernel_main(phys *archive_addr, long archive_size, long machdep0, long machdep1)
{
#if defined(CONFIG_EARLY_CONSOLE)
	early_console_init();
#endif
	info("Apex " VERSION_STRING " for " CONFIG_MACHINE_NAME "\n");

	dbg("Kernel arguments: 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
	    (long)archive_addr, archive_size, machdep0, machdep1);

	struct bootargs args = {archive_addr, archive_size, machdep0, machdep1};

	/*
	 * Do machine dependent initialisation.
	 */
	machine_init(&args);

	/*
	 * Initialise memory managers.
	 */
	kmem_init();

	/*
	 * Run c++ global constructors.
	 */
	extern void (*const __init_array_start)(), (*const __init_array_end)();
	for (uintptr_t p = (uintptr_t)&__init_array_start;
	     p < (uintptr_t)&__init_array_end;
	     p += sizeof(void(*)()))
		(*(void (**)())p)();

	/*
	 * Initialise kernel core.
	 */
	irq_init();
	vm_init();
	task_init();
	thread_init();
	sch_init();
	timer_init();

	/*
	 * Create boot thread then run idle loop.
	 */
	kthread_create(&boot_thread, &args, PRI_DEFAULT, "boot", MA_NORMAL);
	thread_idle();
}

static void
run_init()
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

	sch_resume(th);
}

static void
boot_thread(void *arg)
{
	struct bootargs *args = arg;

	/*
	 * Initialise filesystem.
	 */
	fs_init();
	if (mount(NULL, "/", "ramfs", 0, NULL) < 0)
		panic("failed to create root file system");
	fs_kinit();
	if (mkdir("/dev", 0) < 0)
		panic("failed to create /dev directory");
	if (mount(NULL, "/dev", "devfs", 0, NULL) < 0)
		panic("failed to mount /dev");

	/*
	 * Initialise drivers.
	 */
	null_init();
	zero_init();
	kmsg_init();
	machine_driver_init(args);

	/*
	 * Create boot directory.
	 */
	if (mkdir("/boot", 0) < 0)
		panic("failed to create /boot directory");

	/*
	 * Mount /boot file system according to config options.
	 */
	if (mount(CONFIG_BOOTDEV, "/boot", CONFIG_BOOTFS, 0, NULL) < 0)
		panic("failed to mount /boot");

	/*
	 * Run init process.
	 */
	run_init();

	/*
	 * Terminate boot thread.
	 */
	thread_terminate(thread_cur());
	sch_testexit();
}
