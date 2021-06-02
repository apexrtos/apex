LDSCRIPT := arch/$(CONFIG_ARCH)/kernel.ld

SOURCES += \
	$(CONFIG_MACHINEDIR)/sys/machine.cpp \
	$(CONFIG_MACHINEDIR)/sys/start.S \

INCLUDE += \
	$(CONFIG_BUILDDIR)/$(APEX_SUBDIR)sys/arch/powerpc

#
# Files which depend on asm_def.h require an explicit dependency
#
$(APEX_SUBDIR)machine/qemu/powerpc/ppce500/sys/start.s: $(APEX_SUBDIR)sys/arch/powerpc/booke/asm_def.h
$(APEX_SUBDIR)machine/qemu/powerpc/ppce500/sys/start.s: $(APEX_SUBDIR)sys/arch/powerpc/e500v2/asm_def.h
