#
# Apex Kernel
#

TYPE := exec
TARGET := apex
FLAGS := -nostartfiles -nostdlib -static
FLAGS += -Wframe-larger-than=384 -Wundef
FLAGS += -fno-pie -no-pie
FLAGS += -z max-page-size=32
FLAGS += $(CONFIG_APEX_CFLAGS)
CFLAGS += $(FLAGS)
CXXFLAGS += $(FLAGS) -nostdinc++ -fno-exceptions -fno-use-cxa-atexit -std=gnu++20
DEFS += -DKERNEL -D_GNU_SOURCE
LIBS := ../libc++/libc++.a ../libcxxrt/libcxxrt.a ../libc/libc.a -lgcc

INCLUDE := \
    include \
    $(CONFIG_APEXDIR)/libc++/include \
    $(CONFIG_APEXDIR)/sys \
    $(CONFIG_APEXDIR) \
    arch/$(CONFIG_ARCH)/include \
    $(CONFIG_BUILDDIR) \
    $(CONFIG_SRCDIR) \

SOURCES := \
    fs/mount.c \
    fs/pipe.c \
    fs/syscalls.cpp \
    fs/util/dirbuf_add.c \
    fs/util/for_each_iov.c \
    fs/vfs.c \
    fs/vnode.c \
    kern/clone.cpp \
    kern/debug.c \
    kern/dma.cpp \
    kern/elf_load.cpp \
    kern/exec.cpp \
    kern/irq.c \
    kern/main.c \
    kern/prctl.cpp \
    kern/proc.c \
    kern/sch.c \
    kern/sig.c \
    kern/syscall_table.c \
    kern/syscalls.cpp \
    kern/syslog.c \
    kern/task.c \
    kern/thread.c \
    kern/timer.c \
    lib/crypto/sha256.cpp \
    lib/errno.c \
    lib/jhash3.c \
    lib/queue.c \
    lib/raise.c \
    lib/string_utils.cpp \
    mem/access.cpp \
    mem/kmem.c \
    mem/page.cpp \
    mem/vm.cpp \
    sync/cond.c \
    sync/futex.c \
    sync/mutex.c \
    sync/rwlock.c \
    sync/semaphore.c \
    sync/spinlock.c \

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
$(APEX_SUBDIR)sys/lib/errno_EXTRA_CFLAGS := -fno-lto
$(APEX_SUBDIR)sys/lib/raise_EXTRA_CFLAGS := -fno-lto
MK += ../libc/libc.mk ../libc++/libc++.mk ../libcxxrt/libcxxrt.mk
