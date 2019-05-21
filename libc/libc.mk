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
	$(CONFIG_APEXDIR)/sys/include \
	src/internal \
	src/include \

SOURCES := \
    src/ctype/iswspace.c \
    src/exit/abort.c \
    src/internal/floatscan.c \
    src/internal/intscan.c \
    src/internal/shgetc.c \
    src/locale/c_locale.c \
    src/math/copysign.c \
    src/math/copysignl.c \
    src/math/fabs.c \
    src/math/fmod.c \
    src/math/fmodl.c \
    src/math/scalbn.c \
    src/math/scalbnl.c \
    src/multibyte/btowc.c \
    src/multibyte/internal.c \
    src/multibyte/mbrtoc16.c \
    src/multibyte/mbrtowc.c \
    src/multibyte/mbtowc.c \
    src/multibyte/wcrtomb.c \
    src/multibyte/wctomb.c \
    src/stdio/__overflow.c \
    src/stdio/__toread.c \
    src/stdio/__towrite.c \
    src/stdio/__uflow.c \
    src/stdio/asprintf.c \
    src/stdio/fprintf.c \
    src/stdio/fputwc.c \
    src/stdio/fwide.c \
    src/stdio/fwrite.c \
    src/stdio/snprintf.c \
    src/stdio/swprintf.c \
    src/stdio/vasprintf.c \
    src/stdio/vfprintf.c \
    src/stdio/vfwprintf.c \
    src/stdio/vsnprintf.c \
    src/stdio/vswprintf.c \
    src/stdlib/atol.c \
    src/stdlib/lldiv.c \
    src/stdlib/qsort.c \
    src/stdlib/strtod.c \
    src/stdlib/strtol.c \
    src/stdlib/wcstod.c \
    src/stdlib/wcstol.c \
    src/string/memchr.c \
    src/string/memcmp.c \
    src/string/memcpy.c \
    src/string/memmove.c \
    src/string/memset.c \
    src/string/stpcpy.c \
    src/string/strchr.c \
    src/string/strchrnul.c \
    src/string/strcmp.c \
    src/string/strcpy.c \
    src/string/strcspn.c \
    src/string/strdup.c \
    src/string/strlcpy.c \
    src/string/strlen.c \
    src/string/strncmp.c \
    src/string/strnlen.c \
    src/string/strspn.c \
    src/string/strstr.c \
    src/string/strtok_r.c \
    src/string/wcschr.c \
    src/string/wcslen.c \
    src/string/wcsnlen.c \
    src/string/wmemchr.c \
    src/string/wmemcmp.c \
    src/string/wmemcpy.c \
    src/string/wmemmove.c \
    src/string/wmemset.c \

$(APEX_SUBDIR)libc/src/string/memcmp_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
$(APEX_SUBDIR)libc/src/string/memcpy_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
$(APEX_SUBDIR)libc/src/string/memmove_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
$(APEX_SUBDIR)libc/src/string/memset_EXTRA_CFLAGS := -O3 -fno-tree-loop-distribute-patterns
