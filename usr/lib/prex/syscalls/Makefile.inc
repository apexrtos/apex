VPATH:=	$(SRCDIR)/usr/lib/prex/syscalls:$(VPATH)

OBJS+=	$(SRCDIR)/usr/arch/$(ARCH)/_systrap.o \
	object_create.o object_destroy.o object_lookup.o \
	msg_send.o msg_receive.o msg_reply.o \
	vm_allocate.o vm_free.o vm_attribute.o vm_map.o \
	task_create.o task_terminate.o task_self.o \
	task_suspend.o task_resume.o task_name.o task_getcap.o task_setcap.o \
	thread_create.o thread_terminate.o thread_load.o thread_self.o \
	thread_yield.o thread_suspend.o thread_resume.o thread_schedparam.o \
	thread_getprio.o thread_setprio.o \
	thread_getpolicy.o thread_setpolicy.o \
	timer_sleep.o timer_alarm.o timer_periodic.o \
	_timer_waitperiod.o timer_waitperiod.o \
	exception_setup.o exception_return.o \
	exception_raise.o exception_wait.o \
	device_open.o device_close.o device_read.o device_write.o \
	device_ioctl.o \
	mutex_init.o mutex_destroy.o mutex_trylock.o mutex_unlock.o \
	_mutex_lock.o mutex_lock.o \
	cond_init.o cond_destroy.o cond_signal.o cond_broadcast.o \
	_cond_wait.o cond_wait.o \
	sem_init.o sem_destroy.o sem_trywait.o sem_post.o sem_getvalue.o \
	_sem_wait.o sem_wait.o \
	sys_log.o sys_info.o sys_panic.o sys_time.o \
	sys_debug.o

