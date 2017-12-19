include $(SRCDIR)/mk/own.mk

TARGET=		dev.o
TYPE=		OBJECT
OBJS-y= 	../arm/start.o

OBJS-$(CONFIG_SERIAL)+=	serial.o
OBJS-$(CONFIG_RTC)+=	rtc.o

include $(SRCDIR)/mk/dev.mk
