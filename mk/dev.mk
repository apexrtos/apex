include $(SRCDIR)/mk/own.mk

INCLUDE=	-I$(SRCDIR) -I$(SRCDIR)/dev/$(ARCH)/include \
		-I$(SRCDIR)/dev/include -I$(SRCDIR)/include

ASFLAGS+=	$(INCLUDE)
CFLAGS+=	$(INCLUDE) -nostdinc -fno-builtin -DKERNEL
CPPFLAGS+=	$(INCLUDE) -DKERNEL
LDFLAGS+=	-static -nostdlib -L$(SRCDIR)/conf
LINTFLAGS+=	-DKERNEL

ifeq ($(DRIVER_BASE),AUTODETECT)
LDFLAGS+=	-r
endif

include $(SRCDIR)/mk/Makefile.inc
