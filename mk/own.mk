ifndef _OWN_MK_
_OWN_MK_=1

include $(SRCDIR)/conf/config.mk

ifndef SRCDIR
@echo "Error: Please run configure at the top of source tree"
exit 1
endif

endif
