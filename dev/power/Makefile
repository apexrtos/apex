include $(SRCDIR)/mk/own.mk

ifeq ($(CONFIG_PM),y)
TARGET=		power.o
TYPE=		OBJECT
OBJS-y=		pm.o
OBJS-$(CONFIG_CPUFREQ)+=	cpufreq.o
endif

include $(SRCDIR)/mk/dev.mk
