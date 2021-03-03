#pragma once

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
bool	u_access_okfor(struct as *, const void *, size_t, int);
bool	k_access_ok(const void *, size_t, int);

/*
 * User access locking
 */
int	u_access_begin();
int	u_access_begin_interruptible();
void	u_access_end();
void	u_access_suspend();
int	u_access_resume(const void *, size_t, int);
bool	u_access_continue(const void *, size_t, int);

/*
 * User access fault detection
 */
bool	u_fault();
void	u_fault_clear();

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
	int lock() const { return u_access_begin(); }
	int interruptible_lock() const { return u_access_begin_interruptible(); }
	void unlock() const { u_access_end(); }
};

} /* namespace a */

std::string_view u_string(const char *, size_t);

inline constexpr a::u_access u_access_lock;

#endif
