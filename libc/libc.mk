#
# APEX C library for kernel & bootloader
#
# All source files in this library have been copied from musl with minimal
# modifications for use in APEX.
#
# TODO: musl has architecture specific optimisations for some algorithms
#	(incl memcpy). They might be worth supporting.
#

TYPE := klib
TARGET := libc.a
CFLAGS += -ffreestanding
CFLAGS += -Wno-parentheses -Wno-unused-function -Wno-unused-variable
CFLAGS += -fno-lto -ffunction-sections -fdata-sections

INCLUDE := \
	$(CONFIG_BUILDDIR) \
	src/internal \

SOURCES := \
    src/stdio/__towrite.c \
    src/stdio/fwrite.c \
    src/stdio/snprintf.c \
    src/stdio/vfprintf.c \
    src/stdio/vsnprintf.c \
    src/stdlib/atol.c \
    src/stdlib/lldiv.c \
    src/stdlib/qsort.c \
    src/string/memchr.c \
    src/string/memcmp.c \
    src/string/memcpy.c \
    src/string/memmove.c \
    src/string/memset.c \
    src/string/stpcpy.c \
    src/string/strchrnul.c \
    src/string/strcmp.c \
    src/string/strcpy.c \
    src/string/strcspn.c \
    src/string/strlcpy.c \
    src/string/strlen.c \
    src/string/strncmp.c \
    src/string/strnlen.c \
    src/string/strspn.c \
    src/string/strtok_r.c \

$(APEX_SUBDIR)libc/src/string/memcmp_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
$(APEX_SUBDIR)libc/src/string/memcpy_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
$(APEX_SUBDIR)libc/src/string/memmove_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
$(APEX_SUBDIR)libc/src/string/memset_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
