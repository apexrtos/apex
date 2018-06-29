#
# APEX Kernel
#

TYPE := exec
TARGET := apex
CFLAGS += -nostartfiles -nostdlib -static
CFLAGS += -Wframe-larger-than=384
CFLAGS += -fno-pie -no-pie
CFLAGS += -z max-page-size=32
CFLAGS += $(CONFIG_APEX_CFLAGS)
CXXFLAGS += $(CFLAGS) -fno-exceptions -fno-rtti -std=gnu++17
DEFS += -DKERNEL -D_GNU_SOURCE
LIBS := ../libc/libc.a -lgcc
LDSCRIPT := arch/$(CONFIG_ARCH)/kernel.ld

INCLUDE := \
    include \
    $(CONFIG_APEXDIR) \
    $(CONFIG_BUILDDIR) \
    arch/$(CONFIG_ARCH)/include \
    . \

SOURCES := \
    fs/mount.c \
    fs/pipe.c \
    fs/syscalls.c \
    fs/util/dirbuf_add.c \
    fs/util/for_each_iov.c \
    fs/vfs.c \
    fs/vnode.c \
    kern/bootinfo.c \
    kern/clone.cpp \
    kern/console.c \
    kern/debug.c \
    kern/device.c \
    kern/elf_load.c \
    kern/exec.cpp \
    kern/irq.c \
    kern/main.c \
    kern/prio.c \
    kern/proc.c \
    kern/sch.c \
    kern/sig.c \
    kern/syscalls.c \
    kern/syslog.c \
    kern/task.c \
    kern/thread.c \
    kern/timer.c \
    lib/errno.c \
    lib/jhash3.c \
    lib/queue.c \
    lib/raise.c \
    mem/access.cpp \
    mem/kmem.c \
    mem/page.cpp \
    mem/vm.cpp \
    sync/cond.c \
    sync/futex.c \
    sync/mutex.c \

# Generic unprotected memory support
ifeq ($(origin CONFIG_MMU),undefined)
ifeq ($(origin CONFIG_MPU),undefined)
SOURCES += mem/unprotected.cpp
endif
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

# Architecture specific kernel sources
ARCHDIR := arch/$(CONFIG_ARCH)
include sys/$(ARCHDIR)/include.mk

# Machine specific kernel sources
MACHINEDIR := $(CONFIG_APEXDIR)/machine/$(CONFIG_MACHINE)/sys
include $(MACHINEDIR)/include.mk

# C library
$(APEX_SUBDIR)sys/lib/errno_EXTRA_CFLAGS := -fno-lto
$(APEX_SUBDIR)sys/lib/raise_EXTRA_CFLAGS := -fno-lto
MK := ../libc/libc.mk
