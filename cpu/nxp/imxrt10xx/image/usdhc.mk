ifneq ($(origin apex/bootimg_rule_exists),undefined)

DEFAULT := y
TYPE := imxrt_boot
TARGET := $(CONFIG_SRCDIR)/$(CONFIG_BOARD)_usdhc

LDSCRIPT := $(CONFIG_APEXDIR)/cpu/nxp/imxrt10xx/image/usdhc_boot.ld
CFLAGS := -fno-pie -O2
CXXFLAGS := -fno-pie -O2

INCLUDE := \
	$(CONFIG_APEXDIR) \
	$(CONFIG_BUILDDIR) \

SOURCES := \
	boot_data.c \
	$(CONFIG_MACHINEDIR)/image/dcd.cpp \
	ivt.c \

IMG := bootimg

$(APEX_SUBDIR)cpu/nxp/imxrt10xx/image/boot_data.o: $(IMG)
$(APEX_SUBDIR)cpu/nxp/imxrt10xx/image/boot_data_EXTRA_CFLAGS := \
    -DIMAGE_SIZE="(CONFIG_LOADER_OFFSET + `stat -c %s $(APEX_SUBDIR)$(IMG)`)"

include cpu/nxp/imxrt10xx/image/imxrt_boot.mk

endif
