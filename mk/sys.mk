include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/sys/arch/$(ARCH)/include \
		-I$(SRCDIR)/sys/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -DKERNEL
CPPFLAGS+=	$(INCLUDE) -DKERNEL
LDFLAGS+=	-static -nostdlib -L$(SRCDIR)/conf
LINTFLAGS+=	-DKERNEL

include $(SRCDIR)/mk/Makefile.inc
