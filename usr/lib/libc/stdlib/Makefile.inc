VPATH:=	$(SRCDIR)/usr/lib/libc/stdlib:$(VPATH)

SRCS+=	abort.c abs.c atexit.c atof.c atoi.c atol.c div.c exit.c \
	getopt.c getsubopt.c strtol.c strtoul.c rand.c random.c \
	getenv.c setenv.c calloc.c qsort.c
