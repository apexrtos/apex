DEFAULT := y
TARGET := apeximg
TYPE := boot_$(CONFIG_BOOTFS)

#
# Bootable image requires bootloader
#
ifneq ($(CONFIG_BOOTABLE_IMAGE),)
    MK := boot/boot.mk
endif
