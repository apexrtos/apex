include $(SRCDIR)/mk/own.mk

TARGET=	platform.o
TYPE=	OBJECT
OBJS=	../i386/locore.o \
	../i386/cpufunc.o \
	../i386/cpu.o \
	../i386/trap.o \
	../i386/context.o \
	interrupt.o \
	clock.o \
	diag.o \
	machdep.o

ifeq ($(CONFIG_MMU),y)
OBJS+=	../i386/mmu.o
endif

include $(SRCDIR)/mk/sys.mk
