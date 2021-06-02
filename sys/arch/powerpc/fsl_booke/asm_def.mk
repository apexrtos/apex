TYPE := asm_def
TARGET := asm_def.h
SOURCES := asm_def.cpp
CXXFLAGS += -std=gnu++20 -S -fno-lto

INCLUDE := \
	$(CONFIG_APEXDIR)/sys \
	../include/$(CONFIG_SUBARCH)
