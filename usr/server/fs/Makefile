include $(SRCDIR)/mk/own.mk

PROG=		fs
SUBDIR=		vfs
OBJS=		./vfs/vfscore.o

ifeq ($(CONFIG_DEVFS),y)
SUBDIR+=	devfs
OBJS+=		./devfs/devfs.o
endif
ifeq ($(CONFIG_RAMFS),y)
SUBDIR+=	ramfs
OBJS+=		./ramfs/ramfs.o
endif
ifeq ($(CONFIG_ARFS),y)
SUBDIR+=	arfs
OBJS+=		./arfs/arfs.o
endif
ifeq ($(CONFIG_FIFOFS),y)
SUBDIR+=	fifofs
OBJS+=		./fifofs/fifofs.o
endif
ifeq ($(CONFIG_FATFS),y)
SUBDIR+=	fatfs
OBJS+=		./fatfs/fatfs.o
endif

include $(SRCDIR)/mk/prog.mk
