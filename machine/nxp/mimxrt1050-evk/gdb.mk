TYPE := custom

.PHONY: gdb
gdb:
	BUILDDIR=$(CONFIG_BUILDDIR) \
	TOOLSDIR=$(CONFIG_APEXDIR)/machine/nxp/mimxrt1050-evk/tools \
	arm-none-eabi-gdb -x $(CONFIG_APEXDIR)/machine/nxp/mimxrt1050-evk/gdbinit
