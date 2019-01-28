/*
 * exec.cpp - exec support
 */

#include <exec.h>

#include <access.h>
#include <cerrno>
#include <debug.h>
#include <elf_load.h>
#include <fcntl.h>
#include <fs.h>
#include <limits.h>
#include <mmap.h>
#include <sch.h>
#include <sig.h>
#include <string.h>
#include <sys/mman.h>
#include <task.h>
#include <thread.h>
#include <unistd.h>
#include <vm.h>

// #define TRACE_EXEC

thread *
exec_into(struct task *t, const char *path, const char *const argv[],
    const char *const envp[])
{
	char buf[64];
	const char *prgv[3]{};

	/* handle /proc/self/exe */
	/* REVISIT: remove this when we support /proc */
	if (!strcmp(path, "/proc/self/exe"))
		path = t->path;

	/* check target path */
	if (auto r = access(path, X_OK); r < 0)
		return (thread *)r;

	/* open target file */
	a::fd fd(path, O_RDONLY);
	if (fd < 0)
		return (thread *)(int)fd;

	/* handle #!<ws>command<ws>arg */
	if (auto r = pread(fd, buf, sizeof buf, 0); r < 0)
		return (thread *)r;
	else
		buf[r == sizeof buf ? sizeof buf - 1 : r] = 0;
	if (buf[0] == '#' && buf[1] == '!') {
		char *sv;
		size_t i = 0;
		const char *ws = " \t";
		strtok_r(buf + 2, "\r\n", &sv);
		for (const char *s = strtok_r(buf + 2, ws, &sv); i < 3 && s;
		    s = strtok_r(0, ws, &sv))
			prgv[i++] = s;
		if (i == 3)
			return (thread *)DERR(-ENOEXEC);
		path = prgv[0];
		fd.open(path, O_RDONLY);
		if (fd < 0)
			return (thread *)(int)fd;
	}

	/* create new address space for task */
	std::unique_ptr<as> as(as_create(getpid()));

	/* load program image into new address space */
	void (*entry)(void);
	unsigned auxv[AUX_CNT];
	void *sp;
	if (auto r = elf_load(as.get(), fd, &entry, auxv, &sp); r < 0)
		return (thread *)r;

	/* build arguments on new stack */
	if ((sp = build_args(as.get(), sp, prgv, argv, envp, auxv)) > (void*)-4096UL)
		return (thread *)sp;

	/* create new main thread */
	struct thread *main;
	if (auto r = thread_createfor(t, &main, sp, MEM_NORMAL, entry, 0); r < 0)
		return (thread *)r;
	sig_exec(t);
	task_path(t, path);
	thread_name(main, "main");

	/* notify file system */
	fd.close();
	fs_exec(t);

	/* terminate all other threads in current task */
	struct thread *th;
	list_for_each_entry(th, &t->threads, task_link) {
		if (th != main)
			thread_terminate(th);
	}

	/* switch to new address space */
	if (t == task_cur())
		as_switch(as.get());
	as_destroy(t->as);
	t->as = as.release();

	/* resume vfork thread if this process was vforked */
	if (t->vfork) {
		thread_resume(t->vfork);
		t->vfork = 0;
	}

#if defined(TRACE_EXEC)
	dbg("Address space for %s\n", path);
	as_dump(t->as);
#endif

	return main;
}

static int
validate_args(const char *const args[])
{
	size_t args_len;
	if ((args_len = u_arraylen((const void**)args, ARG_MAX)) < 0)
		return args_len;
	if (args_len == ARG_MAX)
		return DERR(-E2BIG);
	for (size_t i = 0; i < args_len; ++i) {
		size_t arg_len;
		if ((arg_len = u_strnlen(args[i], ARG_MAX)) < 0)
			return arg_len;
		if (arg_len == ARG_MAX)
			return DERR(-E2BIG);
	}

	return 0;
}

int
sc_execve(const char *path, const char *const argv[], const char *const envp[])
{
	std::lock_guard l(global_sch_lock);
	if (auto r = as_modify_begin(task_cur()->as); r < 0)
		return r;

	/* validate arguments */
	auto validate = [&]{
		ssize_t path_len;
		if (path_len = u_strnlen(path, PATH_MAX); path_len < 0)
			return path_len;
		if (path_len == PATH_MAX)
			return DERR(-ENAMETOOLONG);
		if (auto r = validate_args(argv); r < 0)
			return r;
		if (auto r = validate_args(envp); r < 0)
			return r;
		return 0;
	};
	if (auto r = validate(); r < 0) {
		as_modify_end(task_cur()->as);
		return r;
	}

	struct thread *main;
	if ((main = exec_into(task_cur(), path, argv, envp)) > (void*)-4096UL) {
		as_modify_end(task_cur()->as);
		return (int)main;
	}
	thread_resume(main);

	/* no as_modify_end() - the address space on which the lock was taken
	   has been destroyed by exec_into, as_destroy releases the lock */

	return 0;
}
