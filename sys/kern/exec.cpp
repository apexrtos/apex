/*
 * exec.cpp - exec support
 */

#include <exec.h>

#include <access.h>
#include <as.h>
#include <cerrno>
#include <climits>
#include <cstring>
#include <debug.h>
#include <elf_load.h>
#include <fcntl.h>
#include <fs.h>
#include <mmap.h>
#include <sch.h>
#include <sig.h>
#include <sys/mman.h>
#include <syscalls.h>
#include <task.h>
#include <thread.h>
#include <unistd.h>

// #define TRACE_EXEC

expect<thread *>
exec_into(task *t, const char *path, const char *const argv[],
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
		return std::errc{-r};

	/* open target file */
	a::fd fd(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return std::errc{-fd};

	/* handle #!<ws>command<ws>arg */
	if (auto r = pread(fd, buf, sizeof buf, 0); r < 0)
		return std::errc{-r};
	else buf[r == sizeof buf ? sizeof buf - 1 : r] = 0;
	if (buf[0] == '#' && buf[1] == '!') {
		char *sv;
		size_t i = 0;
		const char *ws = " \t";
		strtok_r(buf + 2, "\r\n", &sv);
		for (const char *s = strtok_r(buf + 2, ws, &sv); i < 3 && s;
		    s = strtok_r(0, ws, &sv))
			prgv[i++] = s;
		if (i == 3)
			return DERR(std::errc::executable_format_error);
		path = prgv[0];
		fd.open(path, O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			return std::errc{-fd};
	}

	/* create new address space for task */
	std::unique_ptr<as> as(as_create(getpid()));

	/* load program image into new address space */
	elf_load_result e;
	if (auto r = elf_load(as.get(), fd); !r.ok())
		return r.err();
	else e = r.val();

	/* build arguments on new stack */
	if (auto r = build_args(as.get(), e.sp, prgv, argv, envp, e.auxv);
	    !r.ok())
		return r.err();
	else e.sp = r.val();

	/* create new main thread */
	thread *main;
	if (auto r = thread_createfor(t, as.get(), &main, e.sp, MA_NORMAL,
				      e.entry, 0);
	    r < 0)
		return std::errc{-r};

	/* terminate all other threads in current task */
	thread *th;
	list_for_each_entry(th, &t->threads, task_link) {
		if (th != main)
			thread_terminate(th);
	}

	/* wait for threads to finish */
	sch_lock();
	const k_sigset_t sig_mask = sig_block_all();
	bool done;
	while (true) {
		done = true;
		list_for_each_entry(th, &t->threads, task_link) {
			if (th == thread_cur() || th == main)
				continue;
			done = false;
			break;
		}
		if (done)
			break;
		sch_prepare_sleep(&t->thread_event, 0);
		sch_unlock();
		as_modify_end(task_cur()->as);
		sch_continue_sleep();
		as_modify_begin(task_cur()->as);
		sch_lock();
	}
	sig_restore(&sig_mask);
	sch_unlock();

	thread_name(main, "main");
	sig_exec(t);
	task_path(t, path);

	/* fs_exec will close fd as it's marked as CLOEXEC. No point calling
	 * close() as it will return EINTR because the current thread has
	 * been signalled by thread_terminate() */
	fd.release();

	/* notify file system */
	fs_exec(t);

	/* switch to new address space */
	if (t == task_cur())
		as_switch(as.get());
	as_destroy(t->as);
	t->as = as.release();

	/* resume vfork thread if this process was vforked */
	if (t->vfork) {
		sch_resume(t->vfork);
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
	if ((args_len = u_arraylen((const void *const *)args, ARG_MAX)) < 0)
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

	thread *main;
	if (auto r = exec_into(task_cur(), path, argv, envp); !r.ok()) {
		as_modify_end(task_cur()->as);
		return r.sc_rval();
	} else main = r.val();
	sch_resume(main);

	/* no as_modify_end() - the address space on which the lock was taken
	   has been destroyed by exec_into, as_destroy releases the lock */

	return 0;
}
