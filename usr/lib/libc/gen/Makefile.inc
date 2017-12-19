VPATH:=	$(SRCDIR)/usr/lib/libc/gen:$(VPATH)

SRCS+=	__libc_init.c assert.c \
	$(SRCDIR)/usr/arch/$(ARCH)/setjmp.c \
	execl.c execlp.c execle.c execv.c execvp.c \
	isatty.c time.c basename.c dirname.c \
	gethostname.c sethostname.c \
	err.c errx.c verr.c verrx.c \
	warn.c warnx.c vwarn.c vwarnx.c \
	getpagesize.c confstr.c
