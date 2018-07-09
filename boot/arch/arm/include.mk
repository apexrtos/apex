SOURCES += \
    $(ARCHDIR)/$(CONFIG_SUBARCH)_head.S \
    $(ARCHDIR)/crt_init.S \
    $(CONFIG_APEXDIR)/sys/$(ARCHDIR)/$(CONFIG_SUBARCH)/cache.c \

INCLUDE += \
    $(CONFIG_APEXDIR)/sys/$(ARCHDIR)/include/$(CONFIG_SUBARCH) \
