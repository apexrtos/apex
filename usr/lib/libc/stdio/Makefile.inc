VPATH:=	$(SRCDIR)/usr/lib/libc/stdio:$(VPATH)

SRCS+=	clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c fgetc.c \
	fgetpos.c fgets.c fileno.c findfp.c flags.c fopen.c \
	fprintf.c fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
	fseek.c fsetpos.c ftell.c fvwrite.c fwalk.c fwrite.c \
	getc.c getchar.c gets.c getw.c makebuf.c mktemp.c \
	printf.c putc.c putchar.c puts.c putw.c refill.c remove.c rewind.c \
	scanf.c setbuf.c setbuffer.c setvbuf.c snprintf.c sprintf.c \
	sscanf.c stdio.c ungetc.c vfprintf.c \
	vfscanf.c vprintf.c vscanf.c vsnprintf.c vsprintf.c vsscanf.c \
	wsetup.c \
	perror.c tmpfile.c tmpnam.c tempnam.c
