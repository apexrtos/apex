SOURCES += \
    $(ARCHDIR)/backtrace.cpp \
    $(ARCHDIR)/barrier.cpp \
    $(ARCHDIR)/booke/context.cpp \
    $(ARCHDIR)/booke/handler.cpp \
    $(ARCHDIR)/booke/interrupt.cpp \
    $(ARCHDIR)/booke/locore.S \
    $(ARCHDIR)/booke/mmu2.cpp \
    $(ARCHDIR)/elf.cpp \
    $(ARCHDIR)/fsl_booke/cache.cpp \
    $(ARCHDIR)/interrupt.cpp \
    $(ARCHDIR)/mmio.cpp \
    $(ARCHDIR)/stack.cpp \
    $(ARCHDIR)/syscall.cpp \

$(warning work out WTF llvm is doing with code gen)
E500V2_FLAGS := -mcpu=e500v2 -mno-spe -msoft-float
CFLAGS += $(E500V2_FLAGS)
CXXFLAGS += $(E500V2_FLAGS)

#
# Automatically generate definitions for assembly files
#
MK += \
	$(ARCHDIR)/booke/asm_def.mk \
	$(ARCHDIR)/fsl_booke/asm_def.mk \
	$(ARCHDIR)/e500v2/asm_def.mk

#
# Files which depend on asm_def.h require an explicit dependency
#
$(APEX_SUBDIR)sys/$(ARCHDIR)/booke/locore.s: $(APEX_SUBDIR)sys/$(ARCHDIR)/booke/asm_def.h
