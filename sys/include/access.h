#ifndef access_h
#define access_h

#include <types.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Memory access checking
 */
ssize_t	u_strnlen(const char *, size_t);
ssize_t	u_arraylen(const void **, size_t);
bool	u_strcheck(const char *, size_t);
bool	u_access_ok(const void *, size_t, int);
bool	k_access_ok(const void *, size_t, int);

/*
 * User access fault detection
 */
bool	u_fault(void);
void	u_fault_clear(void);

/*
 * Address validation
 */
bool	k_address(const void *);
bool	u_address(const void *);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* access_h */
