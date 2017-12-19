VPATH:=	$(SRCDIR)/usr/lib/posix/signal:$(VPATH)

SRCS+=	__exception.c \
	pause.c sigaction.c signal.c sigpending.c sigprocmask.c \
	sigsuspend.c sigcompat.c siglist.c
