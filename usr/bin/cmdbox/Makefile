include $(SRCDIR)/mk/own.mk

PROG=	cmdbox
SRCS=	main.c cmd_conf.c
CFLAGS+= -DCMDBOX

SRCS-$(CONFIG_CMD_CAL)+=	../cal/cal.c
SRCS-$(CONFIG_CMD_CAT)+=	../cat/cat.c
SRCS-$(CONFIG_CMD_CLEAR)+=	../clear/clear.c
SRCS-$(CONFIG_CMD_CP)+=		../cp/cp.c
SRCS-$(CONFIG_CMD_DATE)+=	../date/date.c
SRCS-$(CONFIG_CMD_DMESG)+=	../dmesg/dmesg.c
SRCS-$(CONFIG_CMD_ECHO)+=	../echo/echo.c
SRCS-$(CONFIG_CMD_FREE)+=	../free/free.c
SRCS-$(CONFIG_CMD_HEAD)+=	../head/head.c
SRCS-$(CONFIG_CMD_HOSTNAME)+=	../hostname/hostname.c
SRCS-$(CONFIG_CMD_KILL)+=	../kill/kill.c
SRCS-$(CONFIG_CMD_LS)+=		../ls/ls.c
SRCS-$(CONFIG_CMD_MKDIR)+=	../mkdir/mkdir.c
SRCS-$(CONFIG_CMD_MOUNT)+=	../mount/mount.c
SRCS-$(CONFIG_CMD_MV)+=		../mv/mv.c
SRCS-$(CONFIG_CMD_NICE)+=	../nice/nice.c
SRCS-$(CONFIG_CMD_PS)+=		../ps/ps.c
SRCS-$(CONFIG_CMD_PWD)+=	../pwd/pwd.c
SRCS-$(CONFIG_CMD_REBOOT)+=	../reboot/reboot.c
SRCS-$(CONFIG_CMD_RM)+=		../rm/rm.c
SRCS-$(CONFIG_CMD_RMDIR)+=	../rmdir/rmdir.c
SRCS-$(CONFIG_CMD_SHUTDOWN)+=	../shutdown/shutdown.c
SRCS-$(CONFIG_CMD_SLEEP)+=	../sleep/sleep.c
SRCS-$(CONFIG_CMD_SYNC)+=	../sync/sync.c
SRCS-$(CONFIG_CMD_TEST)+=	../test/test.c
SRCS-$(CONFIG_CMD_TOUCH)+=	../touch/touch.c
SRCS-$(CONFIG_CMD_UMOUNT)+=	../umount/umount.c
SRCS-$(CONFIG_CMD_UNAME)+=	../uname/uname.c
SRCS-$(CONFIG_CMD_SH)+=		../sh/sh.c ../sh/cd.c ../sh/exec.c ../sh/cmds.c

SRCS+= $(SRCS-y)

include $(SRCDIR)/mk/prog.mk
