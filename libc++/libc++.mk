#
# APEX C++ library for kernel & bootloader
#

TYPE := klib
TARGET := libc++.a
CXXFLAGS += -fno-exceptions

INCLUDE := \
	$(CONFIG_BUILDDIR) \
	$(CONFIG_APEXDIR)/sys/include \

SOURCES := \
	new.cpp \
