#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#if defined(CIRCULAR_BUFFER_DEBUG)
#define CIRCULAR_BUFFER_DEBUG_ASSERT(x) assert(x)
#else
#define CIRCULAR_BUFFER_DEBUG_ASSERT(x)
#endif

/*
 * simple circular buffer container
 *
 * TODO: exception safety
 */
template<typename T, typename S, typename B>
class cb_impl__ : public B {
	using B::capacity_;
	using B::b_;
public:
	typedef T value_type;
	typedef T &reference;
	typedef const T &const_reference;
	typedef ptrdiff_t difference_type;
	typedef S size_type;
	typedef std::make_signed_t<S> ssize_type;
	typedef T *pointer;
	typedef const T *const_pointer;

	template<bool const__>
	class itr__ {
		friend class cb_impl__;

	public:
		typedef std::conditional_t<const__, const cb_impl__, cb_impl__> container;
		typedef typename cb_impl__<T, S, B>::difference_type difference_type;
		typedef typename cb_impl__<T, S, B>::value_type value_type;
		typedef std::conditional_t<const__,
					   cb_impl__<T, S, B>::const_reference,
					   cb_impl__<T, S, B>::reference>
						reference;
		typedef std::conditional_t<const__,
					   cb_impl__<T, S, B>::const_pointer,
					   cb_impl__<T, S, B>::pointer>
						pointer;
		typedef std::random_access_iterator_tag iterator_category;

		itr__()
		: it_{}
		, c_{}
		{ }

		itr__(size_type it, container *buf)
		: it_{it}
		, c_{buf}
		{ }

		itr__(const itr__ &) = default;
		~itr__() = default;
		itr__ &operator=(const itr__ &) = default;

		/* iterator->const_iterator conversion */
		template<bool c = const__, class = std::enable_if_t<c>>
		itr__(const itr__<false> &r)
		: it_{r.it_}
		, c_{r.c_}
		{ }

		std::strong_ordering operator<=>(const itr__ &r) const
		{
			return c_->offset(it_) <=> c_->offset(r.it_);
		}

		bool operator==(const itr__ &r) const
		{
			return it_ == r.it_;
		}

		itr__ &operator++()
		{
			++it_;
			return *this;
		}

		itr__ operator++(int)
		{
			auto tmp{*this};
			++it_;
			return tmp;
		}

		itr__ &operator--()
		{
			--it_;
			return *this;
		}

		itr__ operator--(int)
		{
			auto tmp{*this};
			--it_;
			return tmp;
		}

		itr__ &operator+=(size_type d)
		{
			it_ += d;
			return *this;
		}

		itr__ operator+(size_type d) const
		{
			auto tmp{*this};
			return tmp += d;
		}

		friend itr__ operator+(size_type d, const itr__ &i)
		{
			return i + d;
		}

		itr__ &operator-=(size_type d)
		{
			it_ -= d;
			return *this;
		}

		itr__ operator-(size_type d) const
		{
			auto tmp{*this};
			return tmp -= d;
		}

		difference_type operator-(const itr__<true> &r) const
		{
			return c_->offset(it_) - c_->offset(r.it_);
		}

		reference operator*() const
		{
			return c_->ref(it_);
		}

		pointer operator->() const
		{
			return &c_->ref(it_);
		}

		reference operator[](size_type d) const
		{
			return c_->ref(it_ + d);
		}

	private:
		size_type it_;
		container *c_;
	};

	typedef itr__<false> iterator;
	typedef itr__<true> const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	/*
	 * Constructor for circular_buffer_fixed.
	 */
	cb_impl__()
	: begin_{0}
	, end_{0}
	{
		/* capacity_ must be power of 2 */
		static_assert(capacity_ && !(capacity_ & (capacity_ - 1)));

		/* arithmetic requires size_type to be unsigned */
		static_assert(std::is_unsigned_v<size_type>);
	}

	/*
	 * Constructor for circular_buffer_wrapper.
	 */
	cb_impl__(size_type capacity)
	: B{capacity}
	, begin_{0}
	, end_{0}
	{
		/* capacity_ must be power of 2 */
		assert(capacity_ && !(capacity_ & (capacity_ - 1)));

		/* arithmetic requires size_type to be unsigned */
		static_assert(std::is_unsigned_v<size_type>);
	}

	/*
	 * Constructor for circular_buffer.
	 */
	cb_impl__(size_type capacity, T *buf)
	: B{capacity, buf}
	, begin_{0}
	, end_{0}
	{
		/* capacity_ must be power of 2 */
		assert(capacity_ && !(capacity_ & (capacity_ - 1)));

		/* arithmetic requires size_type to be unsigned */
		static_assert(std::is_unsigned_v<size_type>);
	}

	/* cb_impl__(const cb_impl__ &) */

	~cb_impl__()
	{
		if (!std::is_trivially_destructible_v<T>)
			clear();
	}

	/* cb_impl__ &operator=(const cb_impl__&); */
	/* bool operator==(const cb_impl__&) const; */
	/* bool operator!=(const cb_impl__&) const; */

	iterator begin()
	{
		return {begin_, this};
	}

	const_iterator begin() const
	{
		return {begin_, this};
	}

	const_iterator cbegin() const
	{
		return {begin_, this};
	}

	iterator end()
	{
		return {end_, this};
	}

	const_iterator end() const
	{
		return {end_, this};
	}

	const_iterator cend() const
	{
		return {end_, this};
	}

	reverse_iterator rbegin()
	{
		return begin();
	}

	const_reverse_iterator rbegin() const
	{
		return begin();
	}

	const_reverse_iterator crbegin() const
	{
		return begin();
	}

	reverse_iterator rend()
	{
		return end();
	}

	const_reverse_iterator rend() const
	{
		return end();
	}

	const_reverse_iterator crend() const
	{
		return end();
	}

	reference front()
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(!empty());
		return ref(begin_);
	}

	const_reference front() const
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(!empty());
		return ref(begin_);
	}

	reference back()
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(!empty());
		return ref(end_ - 1);
	}

	const_reference back() const
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(!empty());
		return ref(end_ - 1);
	}

	template<typename ...A> void emplace_front(A &&...a)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(size() != capacity());
		--begin_;
		new(&front()) T(std::forward<A>(a)...);
	}

	template<typename ...A> void emplace_back(A &&...a)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(size() != capacity());
		++end_;
		new(&back()) T(std::forward<A>(a)...);
	}

	void push_front(const T &v)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(size() != capacity());
		--begin_;
		new(&front()) T(v);
	}

	void push_front(T &&v)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(size() != capacity());
		--begin_;
		new(&front()) T(std::move(v));
	}

	void push_back(const T &v)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(size() != capacity());
		++end_;
		new(&back()) T(v);
	}

	void push_back(T &&v)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(size() != capacity());
		++end_;
		new(&back()) T(std::move(v));
	}

	void pop_front()
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(!empty());
		front().~T();
		++begin_;
	}

	void pop_back()
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(!empty());
		back().~T();
		--end_;
	}

	reference operator[](size_type i)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(i < size());
		return ref(begin_ + i);

	}

	const_reference operator[](size_type i) const
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(i < size());
		return ref(begin_ + i);
	}

	/* reference at(size_type); */
	/* const_reference at(size_type) const; */

	template<class ...A> iterator emplace(const_iterator pos, A &&...a)
	{
		const auto it = expand(pos.it_, 1);
		new(&ref(it)) T(std::forward<A>(a)...);
		return {it, this};
	}

	iterator insert(const_iterator pos, const T &v)
	{
		const auto it = expand(pos.it_, 1);
		new(&ref(it)) T(v);
		return {it, this};
	}

	iterator insert(const_iterator pos, T &&v)
	{
		const auto it = expand(pos.it_, 1);
		new(&ref(it)) T(std::move(v));
		return {it, this};
	}

	iterator insert(const_iterator pos, size_type n, const T &v)
	{
		const auto it = expand(pos.it_, n);
		for (size_type i = 0; i != n; ++i)
			new(&ref(it + i)) T(v);
		return {it, this};
	}

	template<class I> iterator insert(const_iterator pos, I b, I e)
	{
		/* memcpy trivially copyable objects */
		if constexpr (std::is_trivially_copyable_v<T> &&
		    std::is_pointer_v<I> &&
		    std::is_convertible_v<const T*, I>) {
			const auto it = expand(pos.it_, e - b);
			auto p = it;
			while (b != e) {
				const auto lin = std::min<size_type>(
						capacity_ - wrap(p), e - b);
				memcpy(&ref(p), b, lin * sizeof(T));
				b += lin;
				p += lin;
			}
			return {it, this};
		}

		const size_type n = e - b;
		const auto it = expand(pos.it_, n);
		for (size_type i = 0; i != n; ++i, ++b)
			new (&ref(it + i)) T(*b);
		return {it, this};
	}

	iterator insert(const_iterator pos, std::initializer_list<T> l)
	{
		return insert(pos, l.begin(), l.end());
	}

	iterator erase(const_iterator pos)
	{
		return {collapse(pos.it_, 1), this};
	}

	iterator erase(const_iterator begin, const_iterator end)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(end >= begin);
		return {collapse(begin.it_, end - begin), this};
	}

	void clear()
	{
		if (std::is_trivially_destructible_v<T>)
			begin_ = end_ = 0;
		else while (!empty())
			pop_back();
	}

	/* template<class it> void assign(iter, iter); */
	/* void assign(std::initializer_list<T>); */
	/* void assign(size_type, const T&); */
	/* void swap(cb_impl__ &); */

	size_type size() const
	{
		return end_ - begin_;
	}

	size_type max_size() const
	{
		return capacity_;
	}

	bool empty() const
	{
		return end_ == begin_;
	}

	/*
	 * Extensions for cb_impl__
	 */

	size_type capacity() const
	{
		return capacity_;
	}

	std::array<std::pair<T *, size_type>, 2> data()
	{
		std::array<std::pair<T *, size_type>, 2> r{};
		auto o = r.begin();
		for (auto it = begin(); it != end(); ++o) {
			const auto lin = linear(it);
			o->first = &*it;
			o->second = lin;
			it += lin;
		}
		return r;
	}

	size_type linear(const_iterator pos)
	{
		return std::min<size_type>(
		    end() - pos, capacity_ - wrap(pos.it_));
	}

	size_type linear(const_iterator pos, const_iterator end)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(end >= pos);
		return std::min<size_type>(
		    end - pos, capacity_ - wrap(pos.it_));
	}

private:
	size_type begin_;
	size_type end_;

	/* get index from iterator */
	size_type wrap(size_type i) const
	{
		return i & (capacity_ - 1);
	}

	/* get offset (distance from start of container) from iterator */
	size_type offset(size_type i) const
	{
		return i - begin_;
	}

	/* get reference to object in buffer */
	reference ref(size_type i)
	{
		return b_[wrap(i)];
	}

	/* get reference to object in buffer */
	const_reference ref(size_type i) const
	{
		return b_[wrap(i)];
	}

	/* construct or move count items from src to dst where src > dst */
	void move_left(size_type dst, size_type src, size_type count)
	{
		/* memmove trivially copyable objects */
		if constexpr (std::is_trivially_copyable_v<T>) {
			while (count) {
				const auto lin = std::min<size_type>(
				    capacity_ - std::max(wrap(dst), wrap(src)),
				    count);
				memmove(&ref(dst), &ref(src), lin * sizeof(T));
				count -= lin;
				src += lin;
				dst += lin;
			}
			return;
		}

		/* construct/move nontrivially copyable objects */
		ssize_type i = 0;
		for (; i < std::min<ssize_type>(begin_ - dst, count); ++i)
			new(&ref(dst + i)) T(std::move(ref(src + i)));
		for (; i < static_cast<ssize_type>(count); ++i)
			ref(dst + i) = std::move(ref(src + i));
	}

	/* construct or move count items from src to dst where src < dst */
	void move_right(size_type dst, size_type src, size_type count)
	{
		/* memmove trivially copyable objects */
		if constexpr (std::is_trivially_copyable_v<T>) {
			src += count;
			dst += count;
			while (count) {
				const auto lin = std::min(
				    std::min<size_type>(wrap(dst) ?: -1, wrap(src) ?: -1),
				    count);
				count -= lin;
				src -= lin;
				dst -= lin;
				memmove(&ref(dst), &ref(src), lin * sizeof(T));
			}
			return;
		}

		/* construct/move nontrivially copyable objects */
		ssize_type i = count - 1;
		for (; i >= std::max<ssize_type>(end_ - dst, 0); --i)
			new(&ref(dst + i)) T(std::move(ref(src + i)));
		for (; i >= 0; --i)
			ref(dst + i) = std::move(ref(src + i));
	}

	size_type expand(size_type begin, size_type len)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(capacity() - size() >= len);

		const size_type end = begin + len;

		if (len == 0)
			return begin;

		/* end */
		if (begin == end_) {
			end_ = end;
			return begin;
		}

		/* beginning */
		if (begin == begin_) {
			begin_ -= len;
			return begin_;
		}

		/* middle */
		const size_type lmoves = wrap(end - begin_) - 1;
		const size_type rmoves = wrap(end_ - begin);
		if (lmoves <= rmoves) {
			move_left(begin_ - len, begin_, lmoves);
			/* destroy unused objects */
			for (size_type i = 0; i != std::min(len, lmoves); ++i)
				ref(begin + i - len).~T();
			begin_ -= len;
			return begin - len;
		} else {
			move_right(end, begin, rmoves);
			/* destroy unused objects */
			for (size_type i = 0; i != std::min(len, rmoves); ++i)
				ref(begin + i).~T();
			end_ += len;
			return begin;
		}
	}

	size_type collapse(size_type begin, size_type len)
	{
		CIRCULAR_BUFFER_DEBUG_ASSERT(len <= size_type(end_ - begin));

		const size_type end = begin + len;

		if (len == 0)
			return begin;

		/* beginning */
		if (begin == begin_) {
			/* destroy unused objects */
			for (size_type i = 0; i != len; ++i)
				ref(begin + i).~T();
			return begin_ += len;
		}

		/* end */
		if (end == end_) {
			/* destroy unused objects */
			for (size_type i = 0; i != len; ++i)
				ref(begin + i).~T();
			return end_ -= len;
		}

		/* middle */
		const size_type lmoves = wrap(end_ - end);
		const size_type rmoves = wrap(begin - begin_);
		if (lmoves <= rmoves) {
			move_left(begin, end, lmoves);
			/* destroy unused objects */
			for (size_type i = 0; i != len; ++i)
				ref(end_ - len + i).~T();
			end_ -= len;
			return begin;
		} else {
			move_right(begin_ + len, begin_, rmoves);
			/* destroy unused objects */
			for (size_type i = 0; i != len; ++i)
				ref(begin_ + i).~T();
			begin_ += len;
			return begin + len;
		}
	}
};

/* template <class T, class S> void swap(cb_impl__<T, S>&, cb_impl__<T, S>&); */

template<typename T, typename S>
class cb_alloc__ {
public:
	cb_alloc__(S capacity)
	: b_{static_cast<T*>(::operator new(sizeof(T) * capacity))}
	, capacity_{capacity}
	{ }

	~cb_alloc__()
	{
		::operator delete(this->b_);
	}

protected:
	T *const b_;
	const S capacity_;
};

template<typename T, typename S>
class cb_wrap__ {
public:
	cb_wrap__(S capacity, T *buf)
	: b_{buf}
	, capacity_{capacity}
	{ }

protected:
	T *const b_;
	const S capacity_;
};

template<typename T, typename S, size_t N>
class cb_fixed__ {
protected:
	T b_[N];
	static constexpr S capacity_ = N;
};

/*
 * circular_buffer
 */
template<typename T, typename S = size_t>
using circular_buffer = cb_impl__<T, S, cb_alloc__<T, S>>;

/*
 * circular_buffer_wrapper
 */
template<typename T, typename S = size_t>
using circular_buffer_wrapper = cb_impl__<T, S, cb_wrap__<T, S>>;

/*
 * circular_buffer_fixed
 */
template<typename T, size_t N, typename S = size_t>
using circular_buffer_fixed = cb_impl__<T, S, cb_fixed__<T, S, N>>;

#undef CIRCULAR_BUFFER_DEBUG_ASSERT
