#ifndef ioctl_h
#define ioctl_h

#include <sys/ioctl.h>

#define _IOC_DIRSHIFT	30
#define _IOC_SIZESHIFT	16
#define _IOC_GROUPSHIFT	8
#define _IOC_NUMSHIFT	0

#define _IOC_DIRBITS	(32 - _IOC_DIRSHIFT)
#define _IOC_SIZEBITS	(32 - _IOC_SIZESHIFT - _IOC_DIRBITS)
#define _IOC_GROUPBITS	(32 - _IOC_GROUPSHIFT - _IOC_SIZEBITS - _IOC_DIRBITS)
#define _IOC_NUMBITS	(32 - _IOC_NUMSHIFT - _IOC_GROUPBITS - _IOC_SIZEBITS - _IOC_DIRBITS)

#define _IOC_DIRMASK	((1 << _IOC_DIRBITS)-1)
#define _IOC_SIZEMASK	((1 << _IOC_SIZEBITS)-1)
#define _IOC_GROUPMASK	((1 << _IOC_GROUPBITS)-1)
#define _IOC_NUMMASK	((1 << _IOC_NUMBITS)-1)


#define _IOC_DIR(ioc)	(((ioc) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_SIZE(ioc)	(((ioc) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)
#define _IOC_GROUP(ioc)	(((ioc) >> _IOC_GROUPSHIFT) & _IOC_GROUPMASK)
#define _IOC_NUM(ioc)	(((ioc) >> _IOC_NUMSHIFT) & _IOC_NUMMASK)

#endif	/* !ioctl_h */
