ifneq ($(origin apex/bootimg_rule_exists),undefined)

DEFAULT := y
TYPE := imxrt_boot
TARGET := $(CONFIG_SRCDIR)/$(CONFIG_BOARD)_flexspi

LDSCRIPT := $(CONFIG_APEXDIR)/cpu/nxp/imxrt10xx/image/flexspi_boot.ld
CFLAGS := -fno-pie -O2
CXXFLAGS := -fno-pie -O2

INCLUDE := \
	$(CONFIG_APEXDIR) \
	$(CONFIG_BUILDDIR) \

SOURCES := \
	xip_boot_data.c \
	$(CONFIG_MACHINEDIR)/image/hyperflash_config.c \
	$(CONFIG_MACHINEDIR)/image/dcd.cpp \
	ivt.c \

IMG := bootimg

include cpu/nxp/imxrt10xx/image/imxrt_boot.mk

endif
