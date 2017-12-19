include $(SRCDIR)/mk/own.mk

TARGET=	gen.o
TYPE=	OBJECT
OBJS-y=	null.o zero.o

OBJS-$(CONFIG_TTY)+=		tty.o
OBJS-$(CONFIG_RAMDISK)+=	ramdisk.o

include $(SRCDIR)/mk/dev.mk
