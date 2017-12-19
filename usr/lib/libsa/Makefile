LIB=	$(SRCDIR)/usr/lib/libsa.a

include $(SRCDIR)/mk/own.mk

SRCS=	__libc_init.c _exit.c _stdio.c getchar.c \
	putchar.c puts.c printf.c \
	vsprintf.c assert.c sprintf.c

CFLAGS+=	-D_STANDALONE
CPPFLAGS+=	-D_STANDALONE

include $(SRCDIR)/usr/lib/prex/Makefile.inc
include $(SRCDIR)/usr/lib/libc/ctype/Makefile.inc
include $(SRCDIR)/usr/lib/libc/errno/Makefile.inc
include $(SRCDIR)/usr/lib/libc/stdlib/Makefile.inc
include $(SRCDIR)/usr/lib/libc/string/Makefile.inc

include $(SRCDIR)/mk/lib.mk
