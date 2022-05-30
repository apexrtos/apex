#
# Apex Kernel
#

# APEX_CFLAGS are optional
ifndef CONFIG_APEX_CFLAGS
CONFIG_APEX_CFLAGS :=
endif
ifndef CONFIG_APEX_CFLAGS_$(COMPILER)
CONFIG_APEX_CFLAGS_$(COMPILER) :=
endif

TYPE := exec
TARGET := apex
OBJDIR := obj/$(subst /,_,$(APEX_SUBDIR)sys/$(TARGET))
FLAGS += -Wframe-larger-than=384 -Wundef
FLAGS += -fno-pie
FLAGS += $(CONFIG_APEX_CFLAGS) $(CONFIG_APEX_CFLAGS_$(COMPILER))
CFLAGS += $(FLAGS)
CXXFLAGS += $(FLAGS) -nostdinc++ -fno-exceptions -std=gnu++20
CXXFLAGS_gcc += -fno-use-cxa-atexit
CXXFLAGS_clang += -fno-c++-static-destructors -Wno-mismatched-tags
LDFLAGS += -z max-page-size=32 -nostartfiles -nostdlib -static -no-pie
DEFS += -DKERNEL -D_GNU_SOURCE
LIBS := ../libc++/libc++.a ../libcxxrt/libcxxrt.a ../libc/libc.a \
    $(shell $(CROSS_COMPILE)$(COMPILER) --print-libgcc-file-name)

INCLUDE := \
    include \
    $(CONFIG_APEXDIR)/libc++/include \
    $(CONFIG_APEXDIR)/sys \
    $(CONFIG_APEXDIR) \
    arch/$(CONFIG_ARCH)/include \
    $(CONFIG_BUILDDIR) \
    $(CONFIG_SRCDIR) \

SOURCES := \
    fs/mount.cpp \
    fs/pipe.cpp \
    fs/syscalls.cpp \
    fs/util/dirbuf_add.cpp \
    fs/vfs.cpp \
    fs/vnode.cpp \
    kern/clone.cpp \
    kern/debug.cpp \
    kern/dma.cpp \
    kern/elf_load.cpp \
    kern/exec.cpp \
    kern/irq.cpp \
    kern/main.cpp \
    kern/prctl.cpp \
    kern/proc.cpp \
    kern/sch.cpp \
    kern/sig.cpp \
    kern/syscall_table.c \
    kern/syscalls.cpp \
    kern/syslog.cpp \
    kern/task.cpp \
    kern/thread.cpp \
    kern/timer.cpp \
    lib/crypto/sha256.cpp \
    lib/errno.cpp \
    lib/jhash3.c \
    lib/queue.cpp \
    lib/raise.cpp \
    lib/string_utils.cpp \
    mem/access.cpp \
    mem/kmem.cpp \
    mem/page.cpp \
    mem/vm.cpp \
    sync/cond.cpp \
    sync/futex.cpp \
    sync/mutex.cpp \
    sync/rwlock.cpp \
    sync/semaphore.cpp \
    sync/spinlock.cpp \

# Generic memory translation support
ifneq ($(origin CONFIG_MMU),undefined)
SOURCES += mem/translated.cpp
else
SOURCES += mem/untranslated.cpp
endif

# Configured file systems. devfs and ramfs are always required.
ifneq ($(origin CONFIG_FS),undefined)
    include $(addsuffix /include.mk,$(CONFIG_FS))
endif
include sys/fs/devfs/include.mk
include sys/fs/ramfs/include.mk

# Configured drivers.
ifneq ($(origin CONFIG_DRIVERS),undefined)
    include $(addsuffix /include.mk,$(CONFIG_DRIVERS))
endif
include sys/dev/null/include.mk
include sys/dev/zero/include.mk
include sys/dev/tty/include.mk
include sys/dev/block/include.mk

# Architecture specific kernel sources
ARCHDIR := arch/$(CONFIG_ARCH)
include sys/$(ARCHDIR)/include.mk

# Machine specific kernel sources
include $(CONFIG_MACHINEDIR)/sys/include.mk

# C library
$(OBJDIR)/$(APEX_SUBDIR)sys/lib/errno_EXTRA_CFLAGS := -fno-lto
$(OBJDIR)/$(APEX_SUBDIR)sys/lib/raise_EXTRA_CFLAGS := -fno-lto
MK += ../libc/libc.mk ../libc++/libc++.mk ../libcxxrt/libcxxrt.mk
