VPATH:=	$(SRCDIR)/usr/lib/libc/termios:$(VPATH)

SRCS+=	tcgetattr.c tcsetattr.c tcgetpgrp.c tcsetpgrp.c \
	cfgetospeed.c cfsetospeed.c cfgetispeed.c cfsetispeed.c cfsetspeed.c \
	cfmakeraw.c tcsendbreak.c tcdrain.c tcflush.c tcflow.c
