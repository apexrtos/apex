ifneq ($(origin apex/apeximg_rule_exists),undefined)

DEFAULT := y
TYPE := imxrt_boot
TARGET := $(CONFIG_SRCDIR)/mimxrt1050-evkb_hyperflash

LDSCRIPT := $(CONFIG_APEXDIR)/cpu/nxp/imxrt10xx/flexspi_boot.ld
CFLAGS := -fno-pie -O2
CXXFLAGS := -fno-pie -O2

INCLUDE := \
	$(CONFIG_APEXDIR) \
	$(CONFIG_BUILDDIR) \

SOURCES := \
	xip_boot_data.c \
	hyperflash_config.c \
	dcd.cpp \
	ivt.c \

IMG := apeximg

include cpu/nxp/imxrt10xx/imxrt_boot.mk

endif
