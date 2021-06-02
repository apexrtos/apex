SOURCES += \
    $(CONFIG_MACHINEDIR)/boot/machine.cpp \
    $(CONFIG_MACHINEDIR)/boot/machine_init.S \
    $(CONFIG_MACHINEDIR)/boot/ns16550/ns16550.cpp \

#
# Files which depend on asm_def.h require an explicit dependency
#
$(APEX_SUBDIR)machine/qemu/powerpc/ppce500/boot/machine_init.s: $(APEX_SUBDIR)sys/arch/powerpc/booke/asm_def.h
