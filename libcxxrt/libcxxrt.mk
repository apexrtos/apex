#
# APEX C++ runtime support library
#
TYPE := klib
TARGET := libcxxrt.a
CXXFLAGS += -nostdinc++ -fno-exceptions

INCLUDE := \
	$(CONFIG_BUILDDIR) \
	$(CONFIG_APEXDIR)/sys/include \

SOURCES := \
	src/auxhelper.cc \
	src/dynamic_cast.cc \
	src/stdexcept.cc \
	src/typeinfo.cc \
