include $(SRCDIR)/mk/own.mk

INCLUDE+=	-I$(SRCDIR) -I$(SRCDIR)/boot/$(ARCH)/include \
		-I$(SRCDIR)/boot/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -DKERNEL -nostdinc -fno-builtin
CPPFLAGS+=	$(INCLUDE) -DKERNEL
LDFLAGS+=	-static -nostdlib
LINTFLAGS+=	-DKERNEL

include $(SRCDIR)/mk/Makefile.inc
