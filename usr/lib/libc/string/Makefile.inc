VPATH:=	$(SRCDIR)/usr/lib/libc/string:$(VPATH)

SRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memccpy.c memchr.c \
	memcmp.c memcpy.c memmove.c memset.c rindex.c strcasecmp.c strcat.c \
	strchr.c strcmp.c strcoll.c strcpy.c strncpy.c strncat.c strcspn.c \
	strlen.c strmode.c strdup.c strncmp.c strerror.c \
	strpbrk.c strrchr.c strsep.c strspn.c strstr.c strtok.c strtok_r.c \
	strxfrm.c swab.c strlcpy.c strlcat.c
