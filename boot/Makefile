include $(SRCDIR)/mk/own.mk

TARGET=		bootldr
TYPE=		BINARY
SUBDIR=		lib $(ARCH) common
OBJS=		./$(ARCH)/$(PLATFORM)/boot.o ./common/common.o
LIBS=		./lib/libboot.a $(LIBGCC_PATH)libgcc.a
LD_SCRIPT=	./$(ARCH)/$(ARCH)/boot.ld
LDFLAGS+=	-L$(SRCDIR)/conf
OBJCOPYFLAGS+=	-I elf32-i386 -O binary -R .note -R .comment -S
#DISASM= 	bootldr.lst

include $(SRCDIR)/mk/boot.mk
