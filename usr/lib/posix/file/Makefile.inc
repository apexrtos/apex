VPATH:=	$(SRCDIR)/usr/lib/posix/file:$(VPATH)

SRCS+=	__file.c \
	mount.c umount.c sync.c \
	access.c creat.c open.c close.c read.c write.c lseek.c rewinddir.c \
	fstat.c stat.c lstat.c fsync.c dup.c dup2.c \
	opendir.c closedir.c readdir.c rename.c chdir.c getcwd.c \
	link.c unlink.c rmdir.c mkdir.c mknod.c chmod.c chown.c \
	umask.c ioctl.c fcntl.c pipe.c
