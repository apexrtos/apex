SOURCES += \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/arch.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/atomic.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/cache.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/context.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/emulate.S \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/exception.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/interrupt.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/io.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/locore.S \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/syscall.cpp \

ifneq ($(origin CONFIG_MPU),undefined)
SOURCES += $(ARCHDIR)/$(CONFIG_SUBARCH)/mpu.cpp
endif

ASFLAGS += -mimplicit-it=thumb

#
# Ask clang not to use FPU registers in critical areas
#
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/context_EXTRA_CFLAGS_clang := -mno-implicit-float
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/exception_EXTRA_CFLAGS_clang := -mno-implicit-float
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/mpu_EXTRA_CFLAGS_clang := -mno-implicit-float
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/syscall_EXTRA_CFLAGS_clang := -mno-implicit-float
$(APEX_SUBDIR)sys/kern/sig_EXTRA_CFLAGS_clang := -mno-implicit-float

#
# Automatically generate offsets for assembly files
#
MK += $(ARCHDIR)/$(CONFIG_SUBARCH)/asm_def.mk
DIR := $(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)
INCLUDE += $(CONFIG_BUILDDIR)/$(DIR)

#
# Files which depend on asm_def.h must have an explicit dependency here
#
$(DIR)/emulate.s: $(DIR)/asm_def.h
$(DIR)/locore.s: $(DIR)/asm_def.h
