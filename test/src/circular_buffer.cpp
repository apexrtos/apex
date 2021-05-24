#define CIRCULAR_BUFFER_DEBUG
#include <sys/lib/circular_buffer.h>

#include <gtest/gtest.h>

#include <cassert>
#include <deque>

namespace {

constexpr auto num_tests = 1000000;
constexpr auto container_size = 128;
using size_type = uint8_t;

class tester {
public:
	static int count;

	tester(int val)
	: this_{this}
	, val_{val}
	{
		++count;
	}

	tester(const tester &r)
	: this_(this)
	, val_{r.val_}
	{
		++count;
	}

	~tester()
	{
		assert(this_ == this);
		--count;
		this_ = nullptr;
		val_ = 0;
	}

	tester& operator=(const tester& r)
	{
		assert(this_ == this);
		assert(r.this_ == &r);
		val_ = r.val_;
		return *this;
	}

	bool operator==(const tester &r) const
	{
		return val_ == r.val_;
	}

	operator int() const
	{
		return val_;
	}

	void check()
	{
		assert(this_ == this);
	}

private:
	const tester *this_;
	int val_;
};

int tester::count;

}

/* sanity checks on iterator */
static_assert(std::is_convertible_v<circular_buffer<int>::const_iterator, circular_buffer<int>::const_iterator>);
static_assert(std::is_convertible_v<circular_buffer<int>::iterator, circular_buffer<int>::iterator>);
static_assert(std::is_convertible_v<circular_buffer<int>::iterator, circular_buffer<int>::const_iterator>);
static_assert(!std::is_convertible_v<circular_buffer<int>::const_iterator, circular_buffer<int>::iterator>);
static_assert(std::is_trivially_copy_constructible_v<circular_buffer<int>::iterator>);
static_assert(std::is_trivially_copy_constructible_v<circular_buffer<int>::const_iterator>);

TEST(circular_buffer, iterator)
{
	std::deque<tester> ref;
	circular_buffer<tester, size_type> dut(container_size);

	for (size_t i = 0; i < container_size / 2; ++i) {
		ref.push_front(i);
		dut.push_front(i);
	}
	for (size_t i = container_size / 2; i < container_size; ++i) {
		ref.push_back(i);
		dut.push_back(i);
	}

	for (size_t i = 0; i != num_tests; ++i) {
		auto idx1 = rand() % container_size;
		auto idx2 = rand() % container_size;
		auto ref_it1 = ref.begin() + idx1;
		auto ref_it2 = ref.end() - idx2;
		auto dut_it1 = dut.begin() + idx1;
		auto dut_it2 = dut.end() - idx2;

		ASSERT_EQ(*ref_it1, *dut_it1);
		ASSERT_EQ(ref_it2 != ref.end(), dut_it2 != dut.end());
		if (ref_it2 != ref.end()) {
			ASSERT_EQ(*ref_it2, *ref_it2);
		}
		ASSERT_EQ(ref_it1 == ref_it2, dut_it1 == dut_it2);
		ASSERT_EQ(ref_it1 != ref_it2, dut_it1 != dut_it2);
		ASSERT_EQ(ref_it1 < ref_it2, dut_it1 < dut_it2);
		ASSERT_EQ(ref_it1 > ref_it2, dut_it1 > dut_it2);
		ASSERT_EQ(ref_it1 <= ref_it2, dut_it1 <= dut_it2);
		ASSERT_EQ(ref_it1 >= ref_it2, dut_it1 >= dut_it2);
		ASSERT_EQ(ref_it1 - ref_it2, dut_it1 - dut_it2);
		ASSERT_EQ(ref_it2 - ref_it1, dut_it2 - dut_it1);

		if (ref_it1 < ref_it2) {
			ASSERT_EQ(ref_it1 + 1 != ref.end(), dut_it1 + 1 != dut.end());
			if (ref_it1 + 1 != ref.end()) {
				ASSERT_EQ(*(ref_it1 + 1), *(dut_it1 + 1));
			}
			ASSERT_EQ((ref_it1 + 1) == ref_it2, (dut_it1 + 1) == dut_it2);
			ASSERT_EQ((ref_it1 + 1) != ref_it2, (dut_it1 + 1) != dut_it2);
			ASSERT_EQ((ref_it1 + 1) < ref_it2, (dut_it1 + 1) < dut_it2);
			ASSERT_EQ((ref_it1 + 1) > ref_it2, (dut_it1 + 1) > dut_it2);
			ASSERT_EQ((ref_it1 + 1) <= ref_it2, (dut_it1 + 1) <= dut_it2);
			ASSERT_EQ((ref_it1 + 1) >= ref_it2, (dut_it1 + 1) >= dut_it2);

			ASSERT_EQ(1 + ref_it1 != ref.end(), 1 + dut_it1 != dut.end());
			if (1 + ref_it1 != ref.end()) {
				ASSERT_EQ(*(1 + ref_it1), *(1 + dut_it1));
			}
			ASSERT_EQ((1 + ref_it1) == ref_it2, (1 + dut_it1) == dut_it2);
			ASSERT_EQ((1 + ref_it1) != ref_it2, (1 + dut_it1) != dut_it2);
			ASSERT_EQ((1 + ref_it1) < ref_it2, (1 + dut_it1) < dut_it2);
			ASSERT_EQ((1 + ref_it1) > ref_it2, (1 + dut_it1) > dut_it2);
			ASSERT_EQ((1 + ref_it1) <= ref_it2, (1 + dut_it1) <= dut_it2);
			ASSERT_EQ((1 + ref_it1) >= ref_it2, (1 + dut_it1) >= dut_it2);

			switch (idx1 % 3) {
			case 0:
				++ref_it1;
				++dut_it1;
				break;
			case 1:
				ref_it1++;
				dut_it1++;
				break;
			case 2:
				ref_it1 += 1;
				dut_it1 += 1;
				break;
			}

			ASSERT_EQ(ref_it1 != ref.end(), dut_it1 != dut.end());
			if (ref_it1 != ref.end()) {
				ASSERT_EQ(*ref_it1, *dut_it1);
			}
			ASSERT_EQ(ref_it1 == ref_it2, dut_it1 == dut_it2);
			ASSERT_EQ(ref_it1 != ref_it2, dut_it1 != dut_it2);
			ASSERT_EQ(ref_it1 < ref_it2, dut_it1 < dut_it2);
			ASSERT_EQ(ref_it1 > ref_it2, dut_it1 > dut_it2);
			ASSERT_EQ(ref_it1 <= ref_it2, dut_it1 <= dut_it2);
			ASSERT_EQ(ref_it1 >= ref_it2, dut_it1 >= dut_it2);
		}

		if (ref_it1 > ref_it2) {
			ASSERT_EQ(*(ref_it1 - 1), *(dut_it1 - 1));
			ASSERT_EQ((ref_it1 - 1) == ref_it2, (dut_it1 - 1) == dut_it2);
			ASSERT_EQ((ref_it1 - 1) != ref_it2, (dut_it1 - 1) != dut_it2);
			ASSERT_EQ((ref_it1 - 1) < ref_it2, (dut_it1 - 1) < dut_it2);
			ASSERT_EQ((ref_it1 - 1) > ref_it2, (dut_it1 - 1) > dut_it2);
			ASSERT_EQ((ref_it1 - 1) <= ref_it2, (dut_it1 - 1) <= dut_it2);
			ASSERT_EQ((ref_it1 - 1) >= ref_it2, (dut_it1 - 1) >= dut_it2);

			ASSERT_EQ(*(-1 + ref_it1), *(-1 + dut_it1));
			ASSERT_EQ((-1 + ref_it1) == ref_it2, (-1 + dut_it1) == dut_it2);
			ASSERT_EQ((-1 + ref_it1) != ref_it2, (-1 + dut_it1) != dut_it2);
			ASSERT_EQ((-1 + ref_it1) < ref_it2, (-1 + dut_it1) < dut_it2);
			ASSERT_EQ((-1 + ref_it1) > ref_it2, (-1 + dut_it1) > dut_it2);
			ASSERT_EQ((-1 + ref_it1) <= ref_it2, (-1 + dut_it1) <= dut_it2);
			ASSERT_EQ((-1 + ref_it1) >= ref_it2, (-1 + dut_it1) >= dut_it2);

			switch (idx1 % 3) {
			case 0:
				--ref_it1;
				--dut_it1;
				break;
			case 1:
				ref_it1--;
				dut_it1--;
				break;
			case 2:
				ref_it1 -= 1;
				dut_it1 -= 1;
				break;
			}

			ASSERT_EQ(*ref_it1, *dut_it1);
			ASSERT_EQ(ref_it1 == ref_it2, dut_it1 == dut_it2);
			ASSERT_EQ(ref_it1 != ref_it2, dut_it1 != dut_it2);
			ASSERT_EQ(ref_it1 < ref_it2, dut_it1 < dut_it2);
			ASSERT_EQ(ref_it1 > ref_it2, dut_it1 > dut_it2);
			ASSERT_EQ(ref_it1 <= ref_it2, dut_it1 <= dut_it2);
			ASSERT_EQ(ref_it1 >= ref_it2, dut_it1 >= dut_it2);
		}
	}
}

TEST(circular_buffer, const_iterator)
{
	std::deque<tester> ref;
	circular_buffer<tester, size_type> dut(container_size);

	for (size_t i = 0; i < container_size / 2; ++i) {
		ref.push_front(i);
		dut.push_front(i);
	}
	for (size_t i = container_size / 2; i < container_size; ++i) {
		ref.push_back(i);
		dut.push_back(i);
	}

	for (size_t i = 0; i != num_tests; ++i) {
		auto idx1 = rand() % container_size;
		auto idx2 = rand() % container_size;
		auto ref_it1 = ref.cbegin() + idx1;
		auto ref_it2 = ref.cend() - idx2;
		auto dut_it1 = dut.cbegin() + idx1;
		auto dut_it2 = dut.cend() - idx2;

		ASSERT_EQ(*ref_it1, *dut_it1);
		ASSERT_EQ(ref_it2 != ref.end(), dut_it2 != dut.end());
		if (ref_it2 != ref.end()) {
			ASSERT_EQ(*ref_it2, *ref_it2);
		}
		ASSERT_EQ(ref_it1 == ref_it2, dut_it1 == dut_it2);
		ASSERT_EQ(ref_it1 != ref_it2, dut_it1 != dut_it2);
		ASSERT_EQ(ref_it1 < ref_it2, dut_it1 < dut_it2);
		ASSERT_EQ(ref_it1 > ref_it2, dut_it1 > dut_it2);
		ASSERT_EQ(ref_it1 <= ref_it2, dut_it1 <= dut_it2);
		ASSERT_EQ(ref_it1 >= ref_it2, dut_it1 >= dut_it2);
		ASSERT_EQ(ref_it1 - ref_it2, dut_it1 - dut_it2);
		ASSERT_EQ(ref_it2 - ref_it1, dut_it2 - dut_it1);

		if (ref_it1 < ref_it2) {
			ASSERT_EQ(ref_it1 + 1 != ref.end(), dut_it1 + 1 != dut.end());
			if (ref_it1 + 1 != ref.end()) {
				ASSERT_EQ(*(ref_it1 + 1), *(dut_it1 + 1));
			}
			ASSERT_EQ((ref_it1 + 1) == ref_it2, (dut_it1 + 1) == dut_it2);
			ASSERT_EQ((ref_it1 + 1) != ref_it2, (dut_it1 + 1) != dut_it2);
			ASSERT_EQ((ref_it1 + 1) < ref_it2, (dut_it1 + 1) < dut_it2);
			ASSERT_EQ((ref_it1 + 1) > ref_it2, (dut_it1 + 1) > dut_it2);
			ASSERT_EQ((ref_it1 + 1) <= ref_it2, (dut_it1 + 1) <= dut_it2);
			ASSERT_EQ((ref_it1 + 1) >= ref_it2, (dut_it1 + 1) >= dut_it2);

			ASSERT_EQ(1 + ref_it1 != ref.end(), 1 + dut_it1 != dut.end());
			if (1 + ref_it1 != ref.end()) {
				ASSERT_EQ(*(1 + ref_it1), *(1 + dut_it1));
			}
			ASSERT_EQ((1 + ref_it1) == ref_it2, (1 + dut_it1) == dut_it2);
			ASSERT_EQ((1 + ref_it1) != ref_it2, (1 + dut_it1) != dut_it2);
			ASSERT_EQ((1 + ref_it1) < ref_it2, (1 + dut_it1) < dut_it2);
			ASSERT_EQ((1 + ref_it1) > ref_it2, (1 + dut_it1) > dut_it2);
			ASSERT_EQ((1 + ref_it1) <= ref_it2, (1 + dut_it1) <= dut_it2);
			ASSERT_EQ((1 + ref_it1) >= ref_it2, (1 + dut_it1) >= dut_it2);

			switch (idx1 % 3) {
			case 0:
				++ref_it1;
				++dut_it1;
				break;
			case 1:
				ref_it1++;
				dut_it1++;
				break;
			case 2:
				ref_it1 += 1;
				dut_it1 += 1;
				break;
			}

			ASSERT_EQ(ref_it1 != ref.end(), dut_it1 != dut.end());
			if (ref_it1 != ref.end()) {
				ASSERT_EQ(*ref_it1, *dut_it1);
			}
			ASSERT_EQ(ref_it1 == ref_it2, dut_it1 == dut_it2);
			ASSERT_EQ(ref_it1 != ref_it2, dut_it1 != dut_it2);
			ASSERT_EQ(ref_it1 < ref_it2, dut_it1 < dut_it2);
			ASSERT_EQ(ref_it1 > ref_it2, dut_it1 > dut_it2);
			ASSERT_EQ(ref_it1 <= ref_it2, dut_it1 <= dut_it2);
			ASSERT_EQ(ref_it1 >= ref_it2, dut_it1 >= dut_it2);
		}

		if (ref_it1 > ref_it2) {
			ASSERT_EQ(*(ref_it1 - 1), *(dut_it1 - 1));
			ASSERT_EQ((ref_it1 - 1) == ref_it2, (dut_it1 - 1) == dut_it2);
			ASSERT_EQ((ref_it1 - 1) != ref_it2, (dut_it1 - 1) != dut_it2);
			ASSERT_EQ((ref_it1 - 1) < ref_it2, (dut_it1 - 1) < dut_it2);
			ASSERT_EQ((ref_it1 - 1) > ref_it2, (dut_it1 - 1) > dut_it2);
			ASSERT_EQ((ref_it1 - 1) <= ref_it2, (dut_it1 - 1) <= dut_it2);
			ASSERT_EQ((ref_it1 - 1) >= ref_it2, (dut_it1 - 1) >= dut_it2);

			ASSERT_EQ(*(-1 + ref_it1), *(-1 + dut_it1));
			ASSERT_EQ((-1 + ref_it1) == ref_it2, (-1 + dut_it1) == dut_it2);
			ASSERT_EQ((-1 + ref_it1) != ref_it2, (-1 + dut_it1) != dut_it2);
			ASSERT_EQ((-1 + ref_it1) < ref_it2, (-1 + dut_it1) < dut_it2);
			ASSERT_EQ((-1 + ref_it1) > ref_it2, (-1 + dut_it1) > dut_it2);
			ASSERT_EQ((-1 + ref_it1) <= ref_it2, (-1 + dut_it1) <= dut_it2);
			ASSERT_EQ((-1 + ref_it1) >= ref_it2, (-1 + dut_it1) >= dut_it2);

			switch (idx1 % 3) {
			case 0:
				--ref_it1;
				--dut_it1;
				break;
			case 1:
				ref_it1--;
				dut_it1--;
				break;
			case 2:
				ref_it1 -= 1;
				dut_it1 -= 1;
				break;
			}

			ASSERT_EQ(*ref_it1, *dut_it1);
			ASSERT_EQ(ref_it1 == ref_it2, dut_it1 == dut_it2);
			ASSERT_EQ(ref_it1 != ref_it2, dut_it1 != dut_it2);
			ASSERT_EQ(ref_it1 < ref_it2, dut_it1 < dut_it2);
			ASSERT_EQ(ref_it1 > ref_it2, dut_it1 > dut_it2);
			ASSERT_EQ(ref_it1 <= ref_it2, dut_it1 <= dut_it2);
			ASSERT_EQ(ref_it1 >= ref_it2, dut_it1 >= dut_it2);
		}
	}
}

TEST(circular_buffer, nontrivial_insertion_deletion)
{
	int val = 0;

	std::deque<tester> ref;
	circular_buffer<tester, size_type> dut(container_size);

	auto verify = [&]{
		ASSERT_EQ(ref.size(), dut.size());
		ASSERT_EQ(ref.size() * 2, tester::count);
		ASSERT_EQ(ref.empty(), dut.empty());
		ASSERT_EQ(ref.empty(), std::as_const(dut).empty());
		if (!ref.empty()) {
			ASSERT_EQ(ref.front(), dut.front());
			ASSERT_EQ(ref.front(), std::as_const(dut).front());
			ASSERT_EQ(ref.back(), dut.back());
			ASSERT_EQ(ref.back(), std::as_const(dut).back());
		}
		for (size_t i = 0; i < ref.size(); ++i) {
			ASSERT_EQ(ref[i], dut[i]);
			ASSERT_EQ(ref[i], std::as_const(dut)[i]);
			dut[i].check();
		}
		size_t cnt = 0;
		for (auto &v : dut)
			ASSERT_EQ(v, ref[cnt++]);
		auto d = dut.data();
		for (size_t i = 0; i < ref.size(); ++i) {
			if (i < d[0].second)
				ASSERT_EQ(ref[i], d[0].first[i]);
			else
				ASSERT_EQ(ref[i], d[1].first[i - d[0].second]);
		}
	};

	verify();

	for (size_t i = 0; i != num_tests; ++i) {
		switch (rand() % 13) {
		case 0:
			if (ref.size() < container_size) {
				++val;
				ref.push_back(val);
				dut.push_back(val);
			}
			break;
		case 1:
			if (ref.size() < container_size) {
				++val;
				ref.push_front(val);
				dut.push_front(val);
			}
			break;
		case 2:
			if (!ref.empty()) {
				ref.pop_back();
				dut.pop_back();
			}
			break;
		case 3:
			if (!ref.empty()) {
				ref.pop_front();
				dut.pop_front();
			}
			break;
		case 4:
			if (ref.size() < container_size) {
				++val;
				ref.emplace_back(val);
				dut.emplace_back(val);
			}
			break;
		case 5:
			if (ref.size() < container_size) {
				++val;
				ref.emplace_front(val);
				dut.emplace_front(val);
			}
			break;
		case 6:
			if (ref.size() < container_size) {
				++val;
				auto idx = rand() % (ref.size() + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.insert(ref_pos, val);
				auto dut_res = dut.insert(dut_pos, val);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		case 7:
			if (ref.size() < container_size) {
				++val;
				auto idx = rand() % (ref.size() + 1);
				auto cnt = rand() % (container_size - ref.size()) ?: 1;
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.insert(ref_pos, cnt, val);
				auto dut_res = dut.insert(dut_pos, cnt, val);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		case 8:
			if (!ref.empty()) {
				auto idx = rand() % ref.size();
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.erase(ref_pos);
				auto dut_res = dut.erase(dut_pos);
				ASSERT_EQ(ref_res != ref.end(), dut_res != dut.end());
				if (ref_res != ref.end()) {
					ASSERT_EQ(*ref_res, *dut_res);
				}
			}
			break;
		case 9:
			if (!ref.empty()) {
				auto idx = rand() % ref.size();
				auto cnt = rand() % (ref.size() - idx + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.erase(ref_pos, ref_pos + cnt);
				auto dut_res = dut.erase(dut_pos, dut_pos + cnt);
				ASSERT_EQ(ref_res != ref.end(), dut_res != dut.end());
				if (ref_res != ref.end()) {
					ASSERT_EQ(*ref_res, *dut_res);
				}
			}
			break;
		case 10:
			ref.clear();
			dut.clear();
			break;
		case 11:
			if (ref.size() < container_size) {
				++val;
				auto idx = rand() % (ref.size() + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.emplace(ref_pos, val);
				auto dut_res = dut.emplace(dut_pos, val);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		case 12:
			if (ref.size() < container_size - 1) {
				const std::initializer_list<tester> d = {++val, ++val};
				auto idx = rand() % (ref.size() + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.insert(ref_pos, d);
				auto dut_res = dut.insert(dut_pos, d);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		}
		verify();
	}

	ref.clear();
	dut.clear();
	verify();
}

TEST(circular_buffer, trivial_insertion_deletion)
{
	int val = 0;

	std::deque<int> ref;
	circular_buffer<int, size_type> dut(container_size);

	auto verify = [&]{
		ASSERT_EQ(ref.size(), dut.size());
		ASSERT_EQ(ref.empty(), dut.empty());
		ASSERT_EQ(ref.empty(), std::as_const(dut).empty());
		if (!ref.empty()) {
			ASSERT_EQ(ref.front(), dut.front());
			ASSERT_EQ(ref.front(), std::as_const(dut).front());
			ASSERT_EQ(ref.back(), dut.back());
			ASSERT_EQ(ref.back(), std::as_const(dut).back());
		}
		for (size_t i = 0; i < ref.size(); ++i) {
			ASSERT_EQ(ref[i], dut[i]);
			ASSERT_EQ(ref[i], std::as_const(dut)[i]);
		}
		size_t cnt = 0;
		for (auto &v : dut)
			ASSERT_EQ(v, ref[cnt++]);
		auto d = dut.data();
		for (size_t i = 0; i < ref.size(); ++i) {
			if (i < d[0].second)
				ASSERT_EQ(ref[i], d[0].first[i]);
			else
				ASSERT_EQ(ref[i], d[1].first[i - d[0].second]);
		}
	};

	verify();

	for (size_t i = 0; i != num_tests; ++i) {
		switch (rand() % 13) {
		case 0:
			if (ref.size() < container_size) {
				++val;
				ref.push_back(val);
				dut.push_back(val);
			}
			break;
		case 1:
			if (ref.size() < container_size) {
				++val;
				ref.push_front(val);
				dut.push_front(val);
			}
			break;
		case 2:
			if (!ref.empty()) {
				ref.pop_back();
				dut.pop_back();
			}
			break;
		case 3:
			if (!ref.empty()) {
				ref.pop_front();
				dut.pop_front();
			}
			break;
		case 4:
			if (ref.size() < container_size) {
				++val;
				ref.emplace_back(val);
				dut.emplace_back(val);
			}
			break;
		case 5:
			if (ref.size() < container_size) {
				++val;
				ref.emplace_front(val);
				dut.emplace_front(val);
			}
			break;
		case 6:
			if (ref.size() < container_size) {
				++val;
				auto idx = rand() % (ref.size() + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.insert(ref_pos, val);
				auto dut_res = dut.insert(dut_pos, val);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		case 7:
			if (ref.size() < container_size) {
				++val;
				auto idx = rand() % (ref.size() + 1);
				auto cnt = rand() % (container_size - ref.size()) ?: 1;
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.insert(ref_pos, cnt, val);
				auto dut_res = dut.insert(dut_pos, cnt, val);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		case 8:
			if (!ref.empty()) {
				auto idx = rand() % ref.size();
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.erase(ref_pos);
				auto dut_res = dut.erase(dut_pos);
				ASSERT_EQ(ref_res != ref.end(), dut_res != dut.end());
				if (ref_res != ref.end()) {
					ASSERT_EQ(*ref_res, *dut_res);
				}
			}
			break;
		case 9:
			if (!ref.empty()) {
				auto idx = rand() % ref.size();
				auto cnt = rand() % (ref.size() - idx + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.erase(ref_pos, ref_pos + cnt);
				auto dut_res = dut.erase(dut_pos, dut_pos + cnt);
				ASSERT_EQ(ref_res != ref.end(), dut_res != dut.end());
				if (ref_res != ref.end()) {
					ASSERT_EQ(*ref_res, *dut_res);
				}
			}
			break;
		case 10:
			ref.clear();
			dut.clear();
			break;
		case 11:
			if (ref.size() < container_size) {
				++val;
				auto idx = rand() % (ref.size() + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.emplace(ref_pos, val);
				auto dut_res = dut.emplace(dut_pos, val);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		case 12:
			if (ref.size() < container_size - 1) {
				/* REVISIT: use initialiser list */
				const std::initializer_list<int> d = {++val, ++val};
				auto idx = rand() % (ref.size() + 1);
				auto ref_pos = ref.begin() + idx;
				auto dut_pos = dut.begin() + idx;
				auto ref_res = ref.insert(ref_pos, d);
				auto dut_res = dut.insert(dut_pos, d);
				ASSERT_EQ(*ref_res, *dut_res);
			}
			break;
		}
		verify();
	}

	ref.clear();
	dut.clear();
	verify();
}

