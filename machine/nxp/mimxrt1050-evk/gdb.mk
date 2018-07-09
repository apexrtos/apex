TYPE := custom

.PHONY: gdb
gdb:
	arm-none-eabi-gdb -x $(CONFIG_APEXDIR)/machine/nxp/mimxrt1050-evk/gdbinit
