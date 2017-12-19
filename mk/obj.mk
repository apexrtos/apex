include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)/usr/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc
CPPFLAGS+=	$(INCLUDE) -nostdinc
LDFLAGS+=	-static $(USR_LDFLAGS)

TYPE=		OBJECT

ifdef SRCS
OBJS+= $(SRCS:.c=.o)
endif

include $(SRCDIR)/mk/Makefile.inc
