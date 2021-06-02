TYPE := asm_def
TARGET := asm_def.h
SOURCES := asm_def.cpp
CXXFLAGS += -std=gnu++20 -S -fno-lto

INCLUDE := \
    $(CONFIG_APEXDIR)/sys/include \
    $(CONFIG_APEXDIR)/sys \
    ../include \
    ../include/$(CONFIG_SUBARCH) \
    $(CONFIG_BUILDDIR) \

define fn_asm_def_rule
    # fn_asm_def_rule

    $$(eval $$(call fn_process_sources))
    $(tgt): $$($(tgt)_OBJS)
	sed '/.*#__OUT__/!d; s///; s/\#//2' $$^ > $$@
endef
