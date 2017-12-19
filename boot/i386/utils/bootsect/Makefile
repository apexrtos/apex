TARGET=		bootsect.bin
TYPE=		BINARY
OBJS=		bootsect.o
LD_SCRIPT=	bootsect.ld
OBJCOPYFLAGS+=	-I elf32-i386 -O binary -R .note -R .comment -S

include $(SRCDIR)/mk/boot.mk
