#
# Makefile.inc - common make rules to build Prex
#

#
# Supported environment variables
#
#  SRCDIR        ... Root directory of source tree
#  ARCH          ... Architecture name
#  PLATFORM      ... Platform name
#  NDEBUG        ... 0 for debug, 1 for release (default: 0)
#  LIBGCC_PATH   ... Directory for libgcc.a
#  CROSS_COMPILE ... Prefix of tools for cross compile
#
# Variables in local Makefile
#
#  TARGET	... Target file name
#  TYPE		... Traget type
#		    e.g. OBJECT,LIBRARY,KERNEL,BINARY,EXEC,DRIVER,OS_IMAGE
#  SUBDIR	... List of subdirectories
#  OBJS		... Object files
#  OBJS-y	... Object files (for drivers)
#  LIBS		... Libraries
#  MAP		... Name of map file
#  DISASM	... Disassemble list file
#  SYMBOL	... Symbol files
#

#
# Option for cross compile
#
#CROSS_COMPILE=
#CROSS_COMPILE=	i386-elf-
#CROSS_COMPILE=	arm-elf-
#CROSS_COMPILE=	powerpc-eabi-
#CROSS_COMPILE=	sh-elf-
#CROSS_COMPILE=	mips-elf-

#
# Tools
#
CC=		$(CROSS_COMPILE)gcc
CPP=		$(CROSS_COMPILE)cpp
AS=		$(CROSS_COMPILE)as
LD=		$(CROSS_COMPILE)ld
AR=		$(CROSS_COMPILE)ar
OBJCOPY=	$(CROSS_COMPILE)objcopy
OBJDUMP=	$(CROSS_COMPILE)objdump
STRIP=		$(CROSS_COMPILE)strip
LINT=		splint
MAKE=		make
#SHELL=		/bin/sh
ifdef SHELL_PATH
SHELL=		$(SHELL_PATH)
endif
ifdef DISASM
ASMGEN=		$(OBJDUMP) $@ --disassemble-all > $(DISASM)
endif

#
# System dependent options
#
include $(SRCDIR)/mk/own.mk

#
# Flags
#
DEFS=		-D__$(ARCH)__ -D__$(PLATFORM)__ \
		-D__ARCH__=$(ARCH) -D__PLATFORM__=$(PLATFORM) \
		-U$(ARCH) -U$(PLATFORM)

CFLAGS+=	-Os -ansi -pedantic -Wall -Wundef -fno-strict-aliasing \
		-Wstrict-prototypes -Wpointer-arith \
		$(CONFIG_CFLAGS) $(DEFS)

ifeq ($(NDEBUG),1)
CFLAGS+=	-fomit-frame-pointer
else
CFLAGS+=	-fno-omit-frame-pointer -g
DEFS+=		-DDEBUG
endif

CPPFLAGS+=	$(DEFS)
ASFLAGS+=	$(DEFS) -D__ASSEMBLY__
ifdef MAP
LDFLAGS+=	-Map $(MAP)
endif
LDFLAGS+=
MAKEFLAGS+=	-rR --no-print-directory
LINTFLAGS+=	-D__lint__ $(DEFS) -weak -nolib -retvalother -fcnuse

BINFLAGS=	-O binary -R .note -R .comment -S

#
# Specify path for libgcc.a
#
ifndef LIBGCC_PATH
LIBGCC_PATH := $(dir $(shell $(CC) $(GLOBAL_CFLAGS) -print-libgcc-file-name))
endif
LIBGCC = $(LIBGCC_PATH)libgcc.a

#
# Inference rules
#
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(AS) -o $@ $<

%.o: %.S
	$(CPP) $(ASFLAGS) $< $*.tmp
	$(AS) -o $@ $*.tmp
	rm -f $*.tmp

#
# Target
#
all: $(SUBDIR) $(TARGET)

#
# Check configuration
#
ifeq (0, ${MAKELEVEL})
# add dependency on config.h
$(SUBDIR):$(SRCDIR)/conf/config.h

$(SRCDIR)/conf/config.h: dummy
	@if [ ! -f $@ ]; then \
		echo 'You must run `configure` before make.' ;\
		exit 1 ;\
	fi
endif

#
# Rules to process sub-directory
#
ifdef SUBDIR
.PHONY: $(SUBDIR)
$(SUBDIR): dummy
	$(MAKE) -C $@
endif

#
# Rules to link a set of .o files into one .o file
#
ifeq ($(TYPE),OBJECT)
$(TARGET): $(OBJS) $(OBJS-y)
	$(LD) $(LDFLAGS) -r -o $@ $^
endif

#
# Rules to compile library
#
ifeq ($(TYPE),LIBRARY)
$(TARGET): $(OBJS) ar-target

.PHONY: ar-target
ar-target: $(OBJS)
	$(AR) rucs $(TARGET) $?
	$(STRIP) -x -R .comment -R .note $(TARGET)
endif

#
# Rules to compile kernel
#
ifeq ($(TYPE),KERNEL)
$(TARGET): $(OBJS) $(LIBS) $(LDS_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(OBJS) $(LIBS) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(STRIP) -s $@
endif

#
# Rules to compile device driver
#
ifeq ($(TYPE),DRIVER)
$(TARGET):  $(OBJS) $(OBJS-y) $(LDS_SCRIPT)
	$(LD) --error-unresolved-symbols $(LDFLAGS) -T $(LD_SCRIPT) -o $@.tmp $(OBJS) $(LIBS) $(LIBGCC)
	rm -f $@.tmp
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(OBJS) $(LIBS) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(STRIP) --strip-debug --strip-unneeded $@
endif

#
# Rules to compile binary file
#
ifeq ($(TYPE),BINARY)
$(TARGET): $(OBJS) $(LDS_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(OBJS) $(LIBS) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(OBJCOPY) $(BINFLAGS) $@
endif

#
# Rules to compile executable file
#
ifeq ($(TYPE),EXEC)
$(TARGET): $(OBJS) $(LIBS) $(LDS_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(CRT0) $(OBJS) $(LIBS) $(LIBC) $(LDADD) $(LIBGCC)
	$(ASMGEN)
ifdef SYMBOL
	cp $@ $(SYMBOL)
endif
	$(STRIP) --strip-debug --strip-unneeded $@
endif

#
# Rules to create OS image
#
ifeq ($(TYPE),OS_IMAGE)
$(TARGET): dummy
ifdef BOOTFILES
	$(AR) rcS ramdisk.a $(BOOTFILES)
	$(AR) t ramdisk.a
	$(AR) rcS tmp.a $(KERNEL) $(DRIVER) $(BOOTTASKS) ramdisk.a
	rm ramdisk.a
else
	$(AR) rcS tmp.a $(KERNEL) $(DRIVER) $(BOOTTASKS)
endif
	$(AR) t tmp.a
	cat $(LOADER) tmp.a > $@
	rm tmp.a
endif


-include Makefile.dep

ifndef SRCS
SRCS = $(OBJS:.o=.c)
endif

#
# Depend
#
.PHONY: depend dep
depend dep:
ifdef SUBDIR
	@(for d in $(SUBDIR) _ ; do \
	  if [ "$$d" != "_" ] ; then $(MAKE) -C $$d depend; fi; \
	done);
endif
	rm -f Makefile.dep
	@(for d in $(SRCS) _ ; do \
	  if [ "$$d" != "_" ] ; then \
	  $(CPP) -M $(CPPFLAGS) $$d >> Makefile.dep; fi; \
	done);
#
# Lint
#
.PHONY: lint
lint:
ifdef SUBDIR
	@(for d in $(SUBDIR) _ ; do \
	  if [ "$$d" != "_" ] ; then $(MAKE) -C $$d lint; fi; \
	done);
endif
	@(for d in $(SRCS) _ ; do \
	  if [ "$$d" != "_" ] ; then \
	  echo ; \
	  echo "Checking $$d" ; \
	  $(LINT) $(LINTFLAGS) $(INCLUDE) $$d; fi; \
	done);

#
# Clean up
#
.PHONY: clean
clean:
ifdef SUBDIR
	@(for d in $(SUBDIR) _ ; do \
	  if [ "$$d" != "_" ] ; then $(MAKE) -C $$d clean; fi; \
	done);
endif
	rm -f Makefile.dep $(TARGET) $(OBJS) $(OBJS-y) $(DISASM) $(MAP) $(SYMBOL) $(CLEANFILES)

.PHONY: dummy
