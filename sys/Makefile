include $(SRCDIR)/mk/own.mk

TARGET=		prex
TYPE=		KERNEL
SUBDIR=		lib arch kern mem ipc sync
OBJS=		./arch/$(ARCH)/$(PLATFORM)/platform.o \
		./kern/kern.o ./mem/mem.o ./ipc/ipc.o ./sync/sync.o
LIBS+=		./lib/libkern.a
LD_SCRIPT=	./arch/$(ARCH)/$(ARCH)/kernel.ld

#MAP=		prex.map
#DISASM= 	prex.lst
#SYMBOL= 	prex.sym

include $(SRCDIR)/mk/sys.mk
