VPATH:=	$(SRCDIR)/usr/lib/posix/gen:$(VPATH)

SRCS+=	__posix_init.c __posix_call.c uname.c syslog.c alarm.c reboot.c
