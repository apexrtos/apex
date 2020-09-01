SOURCES += \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/arch.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/atomic.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/cache.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/context.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/emulate.S \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/exception.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/interrupt.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/io.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/locore.S \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/syscall.c \

ifneq ($(origin CONFIG_MPU),undefined)
SOURCES += $(ARCHDIR)/$(CONFIG_SUBARCH)/mpu.c
endif

ASFLAGS += -mimplicit-it=thumb

#
# Ask gcc not to use FPU registers in critical areas
#
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/context_EXTRA_CFLAGS := -mgeneral-regs-only
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/exception_EXTRA_CFLAGS := -mgeneral-regs-only
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/mpu_EXTRA_CFLAGS := -mgeneral-regs-only
$(APEX_SUBDIR)sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/syscall_EXTRA_CFLAGS := -mgeneral-regs-only
$(APEX_SUBDIR)sys/kern/sig_EXTRA_CFLAGS := -mgeneral-regs-only

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
