include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/usr/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc
CPPFLAGS+=	$(INCLUDE) -nostdinc
LDFLAGS+=	-static $(USR_LDFLAGS)

TYPE=		LIBRARY

ifdef LIB
TARGET ?= $(LIB)
endif

ifdef SRCS
OBJS+= $(SRCS:.c=.o)
endif

include $(SRCDIR)/mk/Makefile.inc
