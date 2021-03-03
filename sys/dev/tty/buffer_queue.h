#pragma once

#include <cassert>
#include <circular_buffer.h>
#include <kernel.h>
#include <page.h>
#include <vector>

/*
 * buffer_queue - tty buffer management
 */
class buffer_queue {
	class entry;
public:
	/*
	 * iterator
	 */
	class iterator {
		friend class buffer_queue;
	public:
		typedef ptrdiff_t difference_type;
		typedef char &reference;
		typedef std::bidirectional_iterator_tag iterator_category;

		iterator()
		: off_{0}
		{ }

		iterator(circular_buffer<entry>::iterator bi, size_t off = 0)
		: bi_{bi}
		, off_{off}
		{ }

		iterator(const iterator &) = default;
		~iterator() = default;
		iterator &operator=(const iterator &) = default;

		bool operator==(const iterator &r) const
		{
			return off_ == r.off_ && bi_ == r.bi_;
		}

		bool operator!=(const iterator &r) const
		{
			return off_ != r.off_ || bi_ != r.bi_;
		}

		bool operator<(const iterator &r) const
		{
			return bi_ < r.bi_ || (bi_ == r.bi_ && off_ < r.off_);
		}

		bool operator>(const iterator &r) const
		{
			return bi_ > r.bi_ || (bi_ == r.bi_ && off_ > r.off_);
		}

		bool operator<=(const iterator &r) const
		{
			return bi_ < r.bi_ || (bi_ == r.bi_ && off_ <= r.off_);
		}

		bool operator>=(const iterator &r) const
		{
			return bi_ > r.bi_ || (bi_ == r.bi_ && off_ >= r.off_);
		}

		iterator &operator++()
		{
			if (++off_ < bi_->len())
				return *this;
			if (bi_->complete()) {
				off_ = 0;
				++bi_;
			}
			return *this;
		}

		iterator operator++(int)
		{
			auto tmp{*this};
			this->operator++();
			return tmp;
		}

		iterator &operator--()
		{
			if (off_ == 0) {
				--bi_;
				off_ = bi_->len() - 1;
				return *this;
			}
			--off_;
			return *this;
		}

		iterator operator--(int)
		{
			auto tmp{*this};
			this->operator--();
			return tmp;
		}

		iterator &operator+=(size_t d)
		{
			while (d && off_ + d >= bi_->len() && bi_->complete()) {
				d -= bi_->len() - off_;
				off_ = 0;
				++bi_;
			}
			off_ += d;
			return *this;
		}

		iterator operator+(size_t d) const
		{
			auto tmp{*this};
			return tmp += d;
		}

		iterator &operator-=(size_t d)
		{
			while (d > off_) {
				d -= off_;
				--bi_;
				off_ = bi_->len();
			}
			off_ -= d;
			return *this;
		}

		iterator operator-(size_t d) const
		{
			auto tmp{*this};
			return tmp -= d;
		}

		difference_type operator-(const iterator &r) const
		{
			/* r must be <= *this for this to work */
			if (r > *this)
				return -(r - *this);
			if (bi_ == r.bi_)
				return off_ - r.off_;
			difference_type diff = r.bi_->len() - r.off_ + off_;
			for (auto it = r.bi_ + 1; it != bi_; ++it)
				diff += it->len();
			return diff;
		}

		reference operator*() const
		{
			return bi_->buf()[off_];
		}

	private:
		circular_buffer<entry>::iterator bi_;
		size_t off_;
	};

	/*
	 * buffer_queue
	 */
	buffer_queue(size_t bufcnt, size_t bufsiz, std::unique_ptr<phys> pages)
	: q_{bufcnt}
	, off_{}
	, bufsiz_{bufsiz}
	, pages_{std::move(pages)}
	{
		/* initialise buffers */
		pool_.reserve(bufcnt);
		for (size_t i = 0; i != bufcnt; ++i)
			pool_.push_back(static_cast<char *>(
			    phys_to_virt(pages_.get() + bufsiz * i)));
	}

	/*
	 * ~buffer_queue
	 */
	~buffer_queue() = default;

	/*
	 * begin - return an interator to the beginning of queued data
	 */
	iterator begin()
	{
		return {q_.begin(), off_};
	}

	/*
	 * end - return an interator to the end of queued data
	 */
	iterator end()
	{
		if (q_.empty() || q_.back().complete())
			return {q_.end(), 0};
		auto last = q_.end() - 1;
		return {last, last->len()};
	}

	/*
	 * push - push a buffer onto the back of the queue
	 */
	void push(char *buf, size_t len)
	{
		if (len == 0)
			pool_.push_back(buf);
		else
			q_.emplace_back(buf, len, true);
	}

	/*
	 * push - push a character onto the back of the queue
	 *
	 * Allocates & pushes buffers as necessary.
	 */
	bool push(char c)
	{
		if (q_.empty() || !q_.back().push(c, bufsiz_)) {
			const auto buf = bufpool_get();
			if (!buf)
				return false;
			q_.emplace_back(buf, 0, false);
			q_.back().push(c, bufsiz_);
		}
		return true;
	}

	/*
	 * copy - copy data from the front of the queue
	 *
	 * It is assumed that the queue size is >= len.
	 */
	void copy(void *buf, size_t len)
	{
		char *cbuf = static_cast<char *>(buf);
		auto it = begin();
		while (len) {
			const auto cp = std::min<size_t>(
			    len, iterator{it.bi_ + 1, 0} - it);
			memcpy(cbuf, &*it, cp);
			len -= cp;
			it += cp;
			cbuf += cp;
		}
	}

	/*
	 * free_buffers_after - free buffers after pos
	 *
	 * Invalidates all iterators referring to locations past pos.
	 */
	void free_buffers_after(iterator pos)
	{
		if (pos.bi_ == q_.end())
			return;
		while (pos.bi_ + 1 != q_.end()) {
			pool_.push_back(q_.back().buf());
			q_.pop_back();
		}
	}

	/*
	 * trim_front - remove data from the front of the queue
	 */
	void trim_front(iterator pos)
	{
		while (q_.begin() != pos.bi_) {
			pool_.push_back(q_.front().buf());
			q_.pop_front();
		}
		off_ = pos.off_;
	}

	/*
	 * expand_if_no_overlap - expand the buffer underlying i1 to it's
	 *			  full size if it does not invalidate i2
	 */
	void expand_if_no_overlap(iterator i1, iterator i2)
	{
		if (i1.bi_ == i2.bi_)
			return;
		i1.bi_->expand(bufsiz_);
	}

	/*
	 * clear - clear all data from the queue
	 */
	void clear()
	{
		while (!q_.empty()) {
			pool_.push_back(q_.back().buf());
			q_.pop_back();
		}
		off_ = 0;
	}

	/*
	 * empty - returns whether the queue is empty
	 */
	bool empty() const
	{
		return q_.empty();
	}

	/*
	 * bufpool_get - retrieve a buffer from the queue
	 */
	char *bufpool_get()
	{
		if (pool_.empty())
			return nullptr;
		auto b = pool_.back();
		pool_.pop_back();
		return b;
	}

	/*
	 * bufpool_empty - returns whether the buffer pool is empty
	 */
	bool bufpool_empty()
	{
		return pool_.empty();
	}

private:
	class entry {
	public:
		entry(char *buf, size_t len, bool complete)
		: buf_{buf}
		, len_{len}
		, complete_{complete}
		{ }

		bool push(char c, size_t bufsiz)
		{
			if (complete_ || len_ == bufsiz)
				return false;
			buf_[len_++] = c;
			if (len_ == bufsiz)
				complete_ = true;
			return true;
		}

		void expand(size_t bufsiz)
		{
			len_ = bufsiz;
		}

		char *buf()
		{
			return buf_;
		}

		size_t len() const
		{
			return len_;
		}

		bool complete() const
		{
			return complete_;
		}

	private:
		char *buf_;
		size_t len_;
		bool complete_;
	};

	circular_buffer<entry> q_;
	size_t off_;
	std::vector<char *> pool_;
	const size_t bufsiz_;
	std::unique_ptr<phys> pages_;	/* pages for receive buffers */
};
