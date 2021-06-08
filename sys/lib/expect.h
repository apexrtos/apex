#pragma once

/*
 * expect - a type which can hold a value or an error
 *
 * Used as a return value for functions which can return a value or an error.
 *
 * In this implementation the error type is limited to the std::errc enum. This
 * is done to limit the range of possible error values, and to use a strongly
 * typed error type.
 *
 * Where appropriate the expect type has an sc_rval() member which returns a
 * value compatible with syscall return.
 */

#include "address.h"
#include <cassert>
#include <system_error>
#include <type_traits>
#include <variant>

/*
 * expect for generic type
 */
template<class T>
class expect {
public:
	expect(T &&v)
	: v_{std::move(v)}
	{ }

	expect(std::errc v)
	: v_{v}
	{ }

	bool
	ok() const
	{
		return holds_alternative<T>(v_);
	}

	T &
	val()
	{
		return std::get<T>(v_);
	}

	const T &
	val() const
	{
		return std::get<T>(v_);
	}

	std::errc
	err() const
	{
		return std::get<std::errc>(v_);
	}

	bool operator==(const expect<T> &) const = default;

private:
	std::variant<std::errc, T> v_;
};

/*
 * expect for pointers
 *
 * NOTE: null pointer is not an error!
 *
 * Reserves a range of pointer values between 0xfffff000 and 0xffffffff for
 * error codes.
 */
template<class T> requires std::is_pointer_v<T>
class expect<T> {
public:
	static_assert(sizeof(T) == sizeof(unsigned long));

	expect(std::nullptr_t)
	: v_{0}
	{ }

	expect(T v)
	: v_{(unsigned long)v}
	{
		assert(ok());
	}

	expect(std::errc v)
	: v_{-(unsigned long)v}
	{
		assert(!ok());
	}

	/* support conversion between pointer types, e.g. char* -> void* */
	template<class U> requires std::is_convertible_v<U, T>
	expect(const expect<U> &v)
	: v_{v.v_}
	{ }

	bool
	ok() const
	{
		return !v_ || -v_ > 4095;
	}

	T
	val()
	{
		assert(ok());
		return (T)v_;
	}

	const T
	val() const
	{
		assert(ok());
		return (T)v_;
	}

	std::errc
	err() const
	{
		assert(!ok());
		return static_cast<std::errc>(-v_);
	}

	long
	sc_rval() const
	{
		return v_;
	}

	bool operator==(const expect<T> &) const = default;

private:
	unsigned long v_;

	template<class U> friend class expect;
};

/*
 * expect for physical addresses
 *
 * NOTE: zero physical address is not an error!
 *
 * Reserves a range of physical addresses between 0xfffff000 and 0xffffffff for
 * error codes.
 */
template<>
class expect<phys> {
public:
	expect(phys v)
	: v_{v.phys()}
	{
		assert(ok());
	}

	expect(std::errc v)
	: v_{-(phys::value_type)v}
	{
		assert(!ok());
	}

	bool
	ok() const
	{
		return !v_ || -v_ > 4095;
	}

	phys
	val()
	{
		assert(ok());
		return phys{v_};
	}

	const phys
	val() const
	{
		assert(ok());
		return phys{v_};
	}

	std::errc
	err() const
	{
		assert(!ok());
		return static_cast<std::errc>(-v_);
	}

	bool operator==(const expect<phys> &) const = default;

private:
	phys::value_type v_;
};

/*
 * expect for positive results
 */
class expect_pos {
public:
	expect_pos(long v)
	: v_{v}
	{
		assert(ok());
	}

	expect_pos(std::errc v)
	: v_{-(long)(v)}
	{
		assert(static_cast<std::underlying_type_t<std::errc>>(v) > 0);
	}

	bool
	ok() const
	{
		return v_ >= 0;
	}

	long
	val() const
	{
		assert(ok());
		return v_;
	}

	std::errc
	err() const
	{
		assert(!ok());
		return static_cast<std::errc>(-v_);
	}

	long
	sc_rval() const
	{
		return v_;
	}

	bool operator==(const expect_pos &) const = default;

private:
	long v_;
};

/*
 * expect for no result
 */
class expect_ok {
public:
	expect_ok()
	: v_{0}
	{}

	expect_ok(std::errc v)
	: v_{-(long)(v)}
	{
		assert(static_cast<std::underlying_type_t<std::errc>>(v) > 0);
	}

	bool
	ok() const
	{
		return v_ == 0;
	}

	std::errc
	err() const
	{
		assert(!ok());
		return static_cast<std::errc>(-v_);
	}

	long
	sc_rval() const
	{
		return v_;
	}

	bool operator==(const expect_ok &) const = default;

private:
	long v_;
};

/*
 * to_errc - convert the return value of a legacy function returning a
 *	     negative error number to an error code
 */
inline
std::errc
to_errc(int r, std::errc ec)
{
	return r < 0 ? std::errc{-r} : ec;
}
