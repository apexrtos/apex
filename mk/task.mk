include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/usr/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -D_STANDALONE
CPPFLAGS+=	$(INCLUDE) -nostdinc -D_STANDALONE
LDFLAGS+=	-static $(USR_LDFLAGS)

LIBC=		$(SRCDIR)/usr/lib/libsa.a
CRT0=		$(SRCDIR)/usr/lib/crt0.o
TYPE=		EXEC

ifeq ($(CONFIG_MMU), y)
LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/user.ld
else
LD_SCRIPT=	$(SRCDIR)/usr/arch/$(ARCH)/user-nommu.ld
LDFLAGS+=	--relocatable -d
endif

ifdef TASK
TARGET ?= $(TASK)
endif

ifndef OBJS
ifdef SRCS
OBJS= $(SRCS:.c=.o)
else
OBJS= $(TARGET).o
endif
endif

include $(SRCDIR)/mk/Makefile.inc
