TARGET=		mkboot.com
TYPE=		BINARY
OBJS=		mkboot.o
LD_SCRIPT=	mkboot.ld
OBJCOPYFLAGS+=	-I elf32-i386 -O binary -R .note -R .comment -S

include $(SRCDIR)/mk/boot.mk
