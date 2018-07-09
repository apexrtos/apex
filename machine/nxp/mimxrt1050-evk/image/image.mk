ifneq ($(origin apex/apeximg_rule_exists),undefined)

DEFAULT := y
TYPE := imxrt_xip
TARGET := $(CONFIG_SRCDIR)/mimxrt1050-evk_apex

LDSCRIPT := $(CONFIG_APEXDIR)/cpu/nxp/imxrt10xx/flexspi_boot.ld
CFLAGS := -fno-pie

INCLUDE := \
	$(CONFIG_APEXDIR) \
	$(CONFIG_BUILDDIR) \

SOURCES := \
	boot_data.c \
	config.c \
	ivt.c \

IMG := apeximg

include cpu/nxp/imxrt10xx/imxrt_xip.mk

endif
