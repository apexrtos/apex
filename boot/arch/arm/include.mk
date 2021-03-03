SOURCES += \
    $(ARCHDIR)/$(CONFIG_SUBARCH)_cache.cpp \
    $(ARCHDIR)/$(CONFIG_SUBARCH)_head.S \
    $(ARCHDIR)/$(CONFIG_SUBARCH)_io.cpp \
    $(ARCHDIR)/crt_init.S \

INCLUDE += \
    $(CONFIG_APEXDIR)/sys/$(ARCHDIR)/include/$(CONFIG_SUBARCH) \
