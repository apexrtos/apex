TYPE := custom

.PHONY: gdb
gdb:
	CPU=$(CONFIG_CPU) \
	BOARD=$(CONFIG_BOARD) \
	BUILDDIR=$(CONFIG_BUILDDIR) \
	TOOLSDIR=$(CONFIG_APEXDIR)/cpu/nxp/imxrt10xx/tools \
	arm-none-eabi-gdb -x $(CONFIG_APEXDIR)/machine/nxp/imxrt1050-evkb/gdbinit
