#ifndef access_h
#define access_h

#include <types.h>

struct as;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Memory access checking
 */
ssize_t	u_strnlen(const char *, size_t);
ssize_t	u_arraylen(const void *const *, size_t);
bool	u_strcheck(const char *, size_t);
bool	u_access_ok(const void *, size_t, int);
bool	u_access_okfor(const struct as *, const void *, size_t, int);
bool	k_access_ok(const void *, size_t, int);

/*
 * User access locking
 */
int	u_access_begin(void);
void	u_access_end(void);
bool	u_access_suspend(void);
int	u_access_resume(bool, const void *, size_t, int);

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
bool	u_addressfor(const struct as *, const void *);

#if defined(__cplusplus)
} /* extern "C" */

#include <string_view>

namespace a {

class u_access {
public:
	int interruptible_lock() const { return u_access_begin(); }
	void unlock() const { u_access_end(); }
};

} /* namespace a */

std::string_view u_string(const char *, size_t);

inline constexpr a::u_access u_access_lock;

#endif

#endif /* access_h */
