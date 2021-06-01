#
# Apex C++ library for kernel & bootloader
#

TYPE := klib
TARGET := libc++.a
CXXFLAGS += -nostdinc++ -fno-exceptions -fno-pic
DEFS := -DLIBCXXRT

INCLUDE := \
	$(CONFIG_BUILDDIR) \
	$(CONFIG_APEXDIR)/sys/include \
	. \
	include \
	../libcxxrt/src \

SOURCES := \
	src/charconv.cpp \
	src/memory.cpp \
	src/new.cpp \
	src/string.cpp \
	src/vector.cpp \
