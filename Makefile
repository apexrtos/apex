include ./conf/config.mk
export SRCDIR
SUBDIR=	boot dev sys usr mk
include $(SRCDIR)/mk/subdir.mk
