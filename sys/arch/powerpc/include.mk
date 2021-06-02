INCLUDE += \
    $(ARCHDIR)/include/$(CONFIG_SUBARCH) \

include sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/include.mk
include sys/$(ARCHDIR)/asm_def_rule.mk
