SOURCES += \
    $(ARCHDIR)/fsl_booke_cache.cpp \
    $(ARCHDIR)/fsl_booke_head.S \
    $(ARCHDIR)/crt_init.S \
    $(ARCHDIR)/mmio.cpp

INCLUDE += \
    $(CONFIG_BUILDDIR)/$(APEX_SUBDIR)sys/arch/powerpc \
    $(CONFIG_APEXDIR)/sys/$(ARCHDIR)/include/e500v2 \
    $(CONFIG_APEXDIR)/sys/$(ARCHDIR)/include \
    $(CONFIG_APEXDIR)/sys

#
# Files which depend on asm_def.h require an explicit dependency
#
$(APEX_SUBDIR)boot/$(ARCHDIR)/fsl_booke_head.s: $(APEX_SUBDIR)sys/$(ARCHDIR)/booke/asm_def.h
$(APEX_SUBDIR)boot/$(ARCHDIR)/fsl_booke_head.s: $(APEX_SUBDIR)sys/$(ARCHDIR)/fsl_booke/asm_def.h
