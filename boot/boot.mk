#
# Boot loader
#

TYPE := binary
TARGET := boot
LDSCRIPT := arch/$(CONFIG_ARCH)/boot.ld
CFLAGS += -nostartfiles -nostdlib -static
CFLAGS += -Wframe-larger-than=384
CFLAGS += -fno-pie -no-pie
CFLAGS += -z max-page-size=32
CFLAGS += $(CONFIG_APEX_CFLAGS)
LIBS := ../libc/libc.a -lgcc

INCLUDE := \
    include \
    $(CONFIG_APEXDIR) \
    $(CONFIG_BUILDDIR) \

SOURCES := \
    errno.c \
    load_bootimg.c \
    load_elf.c \
    main.c \
    raise.c \

# Architecture specific boot sources
ARCHDIR := arch/$(CONFIG_ARCH)
include boot/$(ARCHDIR)/include.mk

# Machine specific boot sources
include $(CONFIG_MACHINEDIR)/boot/include.mk

# C library
$(APEX_SUBDIR)boot/errno_EXTRA_CFLAGS := -fno-lto
$(APEX_SUBDIR)boot/raise_EXTRA_CFLAGS := -fno-lto
MK := ../libc/libc.mk
