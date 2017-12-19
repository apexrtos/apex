VPATH:=	$(SRCDIR)/usr/lib/libc/time:$(VPATH)

SRCS+=	strftime.c gmtime.c gmtime_r.c localtime.c localtime_r.c \
	asctime.c asctime_r.c mktime.c
