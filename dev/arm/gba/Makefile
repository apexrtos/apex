include $(SRCDIR)/mk/own.mk

TARGET=		dev.o
TYPE=		OBJECT
OBJS-y= 	../arm/start.o

OBJS-$(CONFIG_CONSOLE)+=	console.o
OBJS-$(CONFIG_KEYPAD)+=		keypad.o
OBJS-$(CONFIG_KEYBOARD)+=	swkbd.o

include $(SRCDIR)/mk/dev.mk
