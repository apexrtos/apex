SOURCES += \
	$(ARCHDIR)/atomic.cpp \
	$(ARCHDIR)/backtrace.cpp \
	$(ARCHDIR)/cache.cpp \
	$(ARCHDIR)/context.cpp \
	$(ARCHDIR)/elf.cpp \
	$(ARCHDIR)/interrupt.cpp \
	$(ARCHDIR)/locore.S \
	$(ARCHDIR)/mmio.cpp \
	$(ARCHDIR)/stack.cpp \
	$(ARCHDIR)/syscall.cpp \
	$(ARCHDIR)/trap.cpp \

ifdef CONFIG_MPU
SOURCES += $(ARCHDIR)/pmp.cpp
endif

INCLUDE += \
	$(CONFIG_BUILDDIR)/$(APEX_SUBDIR)/sys/$(ARCHDIR)

#
# Automatically generate offsets for assembly files
#
MK += $(ARCHDIR)/asm_def.mk

#
# Files which depend on asm_def.h require an explicit dependency
#
$(APEX_SUBDIR)sys/$(ARCHDIR)/locore.s: $(APEX_SUBDIR)sys/$(ARCHDIR)/asm_def.h
