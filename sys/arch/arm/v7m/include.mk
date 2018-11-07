SOURCES += \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/arch.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/cache.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/context.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/exception.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/interrupt.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/io.c \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/locore.S \
    $(ARCHDIR)/$(CONFIG_SUBARCH)/syscall.c \

ifneq ($(origin CONFIG_MPU),undefined)
SOURCES += $(ARCHDIR)/$(CONFIG_SUBARCH)/mpu.c
endif
