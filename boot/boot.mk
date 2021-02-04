#
# Boot loader
#

# APEX_CFLAGS are optional
ifndef CONFIG_APEX_CFLAGS
CONFIG_APEX_CFLAGS :=
endif
ifndef CONFIG_APEX_CFLAGS_$(COMPILER)
CONFIG_APEX_CFLAGS_$(COMPILER) :=
endif

TYPE := binary
TARGET := boot
LDSCRIPT := arch/$(CONFIG_ARCH)/boot.ld
CFLAGS += -Wframe-larger-than=384
CFLAGS += -fno-pie
CFLAGS += $(CONFIG_APEX_CFLAGS)
CFLAGS_$(COMPILER) += $(CONFIG_APEX_CFLAGS_$(COMPILER))
LDFLAGS += -z max-page-size=32 -nostartfiles -nostdlib -static -no-pie
LIBS := ../libc/libc.a \
    $(shell $(CROSS_COMPILE)$(COMPILER) --print-libgcc-file-name)

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
