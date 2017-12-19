include $(SRCDIR)/mk/own.mk

TARGET=		platform.o
TYPE=		OBJECT
OBJS=		../arm/locore.o ../arm/cpufunc.o interrupt.o \
		../arm/context.o ../arm/trap.o \
		../arm/cpu.o machdep.o clock.o diag.o

ifeq ($(CONFIG_MMU),y)
OBJS+=	../arm/mmu.o
endif

include $(SRCDIR)/mk/sys.mk
