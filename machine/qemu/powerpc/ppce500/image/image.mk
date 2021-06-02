ifneq ($(origin apex/bootimg_rule_exists),undefined)

#
# QEMU's PowerPC e500 platform won't load a raw binary image so we need to
# convert bootimg to an ELF file.
#
# llvm-objcopy can't set section addresses so we need to invoke the linker.
#

DEFAULT := y
TYPE := custom
TARGET := $(CONFIG_BUILDDIR)/$(CONFIG_QEMU_IMG)

.INTERMEDIATE: $(CONFIG_QEMU_IMG).o
$(CONFIG_QEMU_IMG).o: apex/bootimg
	$(CONFIG_CROSS_COMPILE)objcopy -I binary -O elf32-powerpc $< $@

$(CONFIG_QEMU_IMG): apex/machine/qemu/powerpc/ppce500/image/image.ld $(CONFIG_QEMU_IMG).o
	$(CONFIG_CROSS_COMPILE)ld -o $@ -T $< $(CONFIG_QEMU_IMG).o

$(CONFIG_QEMU_IMG)_CLEAN :=

endif
