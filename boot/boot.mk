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
FLAGS += -Wframe-larger-than=384
FLAGS += -fno-pie
FLAGS += $(CONFIG_APEX_CFLAGS) $(CONFIG_APEX_CFLAGS_$(COMPILER))
CFLAGS += $(FLAGS)
CXXFLAGS += $(FLAGS) -nostdinc++ -fno-exceptions -std=gnu++20
CXXFLAGS_gcc += -fno-use-cxa-atexit
CXXFLAGS_clang += -fno-c++-static-destructors
LDFLAGS += -z max-page-size=32 -nostartfiles -nostdlib -static -no-pie
DEFS += -D_GNU_SOURCE
LIBS := ../libc/libc.a \
    $(shell $(CROSS_COMPILE)$(COMPILER) --print-libgcc-file-name)

INCLUDE := \
    include \
    $(CONFIG_APEXDIR)/libc++/include \
    $(CONFIG_APEXDIR) \
    $(CONFIG_BUILDDIR) \
    $(CONFIG_APEXDIR)/sys \

SOURCES := \
    errno.cpp \
    load_bootimg.cpp \
    load_elf.cpp \
    main.cpp \
    raise.cpp \

# Architecture specific boot sources
ARCHDIR := arch/$(CONFIG_ARCH)
include boot/$(ARCHDIR)/include.mk

# Machine specific boot sources
include $(CONFIG_MACHINEDIR)/boot/include.mk

# C library
$(APEX_SUBDIR)boot/errno_EXTRA_CFLAGS := -fno-lto
$(APEX_SUBDIR)boot/raise_EXTRA_CFLAGS := -fno-lto
MK := ../libc/libc.mk
