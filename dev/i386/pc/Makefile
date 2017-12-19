include $(SRCDIR)/mk/own.mk

TARGET=		dev.o
TYPE=		OBJECT
OBJS-y=		../i386/start.o ../i386/cpufunc.o dma.o

OBJS-$(CONFIG_CPUFREQ)+=	../i386/cpu.o
OBJS-$(CONFIG_KEYBOARD)+=	kbd.o
OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_FDD)+=		fdd.o
OBJS-$(CONFIG_MOUSE)+=		mouse.o
OBJS-$(CONFIG_RTC)+=		rtc.o
OBJS-$(CONFIG_SERIAL)+=		serial.o

include $(SRCDIR)/mk/dev.mk
