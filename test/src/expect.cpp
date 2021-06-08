/*
 * Configuration for this test
 */
#define CONFIG_PAGE_OFFSET 0

/*
 * Test victim
 */
#include <sys/lib/expect.h>

/*
 * Test suite
 */
#include <gtest/gtest.h>

TEST(expect, expect_int_ok)
{
	expect<int> e = 0;
	const expect<int> ce = 0;

	EXPECT_TRUE(e.ok());
	EXPECT_TRUE(ce.ok());
	EXPECT_THROW(e.err(), std::exception);
	EXPECT_THROW(ce.err(), std::exception);
	EXPECT_EQ(e.val(), 0);
	EXPECT_EQ(ce.val(), 0);

	e = std::errc::bad_address;
	EXPECT_FALSE(e.ok());
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_THROW(e.val(), std::exception);
}

TEST(expect, expect_int_error)
{
	expect<int> e = std::errc::bad_address;
	const expect<int> ce = std::errc::bad_address;

	EXPECT_FALSE(e.ok());
	EXPECT_FALSE(ce.ok());
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_EQ(ce.err(), std::errc::bad_address);
	EXPECT_THROW(e.val(), std::exception);
	EXPECT_THROW(ce.val(), std::exception);

	e = 17;
	EXPECT_TRUE(e.ok());
	EXPECT_THROW(e.err(), std::exception);
	EXPECT_EQ(e.val(), 17);
}

TEST(expect, expect_unique_ptr_ok)
{
	expect<std::unique_ptr<int>> e = std::unique_ptr<int>{};
	const expect<std::unique_ptr<int>> ce = std::unique_ptr<int>{};

	EXPECT_TRUE(e.ok());
	EXPECT_TRUE(ce.ok());
	EXPECT_THROW(e.err(), std::exception);
	EXPECT_THROW(ce.err(), std::exception);
	EXPECT_EQ(e.val(), std::unique_ptr<int>{});
	EXPECT_EQ(ce.val(), std::unique_ptr<int>{});

	e = std::errc::bad_address;
	EXPECT_FALSE(e.ok());
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_THROW(e.val(), std::exception);
}

TEST(expect, expect_unique_ptr_error)
{
	expect<std::unique_ptr<int>> e = std::errc::bad_address;
	const expect<std::unique_ptr<int>> ce = std::errc::bad_address;

	EXPECT_FALSE(e.ok());
	EXPECT_FALSE(ce.ok());
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_EQ(ce.err(), std::errc::bad_address);
	EXPECT_THROW(e.val(), std::exception);
	EXPECT_THROW(ce.val(), std::exception);

	e = std::unique_ptr<int>{};
	EXPECT_TRUE(e.ok());
	EXPECT_THROW(e.err(), std::exception);
	EXPECT_EQ(e.val(), std::unique_ptr<int>{});
}

TEST(expect, expect_pointer)
{
	expect<void *> e = nullptr;

	auto uintptr_max = std::numeric_limits<uintptr_t>::max();

	auto err_valid_min = std::errc{1};
	auto err_valid_max = std::errc{4095};
	auto ptr_valid_min = nullptr;
	auto ptr_valid_max = reinterpret_cast<void *>(uintptr_max - static_cast<uintptr_t>(err_valid_max));

	auto err_invalid_zero = std::errc{0};
	auto err_invalid_min = std::errc{4096};
	auto err_invalid_max = std::errc(uintptr_max);
	auto ptr_invalid_min = reinterpret_cast<void *>(uintptr_max - static_cast<uintptr_t>(err_valid_max) + 1);
	auto ptr_invalid_max = reinterpret_cast<void *>(uintptr_max);

	EXPECT_DEATH(e = err_invalid_zero, "");
	EXPECT_DEATH(e = err_invalid_min, "");
	EXPECT_DEATH(e = err_invalid_max, "");
	EXPECT_DEATH(e = ptr_invalid_min, "");
	EXPECT_DEATH(e = ptr_invalid_max, "");

	e = ptr_valid_min;
	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), ptr_valid_min);
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), reinterpret_cast<unsigned long>(ptr_valid_min));

	e = ptr_valid_max;
	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), ptr_valid_max);
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), reinterpret_cast<unsigned long>(ptr_valid_max));

	e = err_valid_min;
	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), err_valid_min);
	EXPECT_EQ(e.sc_rval(), -static_cast<unsigned long>(err_valid_min));

	e = err_valid_max;
	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), err_valid_max);
	EXPECT_EQ(e.sc_rval(), -static_cast<unsigned long>(err_valid_max));
}

TEST(expect, pointer_conversion)
{
	expect<void *> ev = nullptr;
	expect<void *> ec = (char *)0x1000;

	EXPECT_EQ(ev.val(), nullptr);

	ev = ec;
	EXPECT_EQ(ev.val(), (void *)0x1000);
}

TEST(expect, expect_pointer_const_ok)
{
	const expect<void *> e = nullptr;

	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), nullptr);
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), 0);
}

TEST(expect, expect_pointer_const_error)
{
	const expect<void *> e = std::errc::bad_address;

	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_EQ(e.sc_rval(), -static_cast<long>(std::errc::bad_address));
}

TEST(expect, phys)
{
	expect<phys> e = 0_phys;

	auto phys_max = std::numeric_limits<phys::value_type>::max();

	auto err_valid_min = std::errc{1};
	auto err_valid_max = std::errc{4095};
	auto phys_valid_min = 0_phys;
	auto phys_valid_max = phys{phys_max - static_cast<phys::value_type>(err_valid_max)};

	auto err_invalid_zero = std::errc{0};
	auto err_invalid_min = std::errc{4096};
	auto err_invalid_max = std::errc(phys_max);
	auto phys_invalid_min = phys{phys_max - static_cast<phys::value_type>(err_valid_max) + 1};
	auto phys_invalid_max = phys{phys_max};

	EXPECT_DEATH(e = err_invalid_zero, "");
	EXPECT_DEATH(e = err_invalid_min, "");
	EXPECT_DEATH(e = err_invalid_max, "");
	EXPECT_DEATH(e = phys_invalid_min, "");
	EXPECT_DEATH(e = phys_invalid_max, "");

	e = phys_valid_min;
	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), phys_valid_min);
	EXPECT_DEATH(e.err(), "");

	e = phys_valid_max;
	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), phys_valid_max);
	EXPECT_DEATH(e.err(), "");

	e = err_valid_min;
	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), err_valid_min);

	e = err_valid_max;
	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), err_valid_max);
}

TEST(expect, phys_const_ok)
{
	const expect<phys> e = 0_phys;

	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), 0_phys);
	EXPECT_DEATH(e.err(), "");
}

TEST(expect, phys_const_error)
{
	const expect<phys> e = std::errc::bad_address;

	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), std::errc::bad_address);
}

TEST(expect, expect_pos)
{
	using errc_type = std::underlying_type_t<std::errc>;

	expect_pos e = 0;

	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), 0);
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), 0);

	EXPECT_DEATH(e = -1, "");
	EXPECT_DEATH(e = std::numeric_limits<long>::min(), "");
	EXPECT_DEATH(e = std::errc{0}, "");
	EXPECT_DEATH(e = std::errc{std::numeric_limits<errc_type>::min()}, "");

	e = std::numeric_limits<long>::max();
	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), std::numeric_limits<long>::max());
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), std::numeric_limits<long>::max());

	e = std::errc{1};
	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), std::errc{1});
	EXPECT_EQ(e.sc_rval(), -1);

	e = std::errc{std::numeric_limits<errc_type>::max()};
	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), std::errc{std::numeric_limits<errc_type>::max()});
	EXPECT_EQ(e.sc_rval(), -std::numeric_limits<errc_type>::max());
}

TEST(expect, expect_pos_const_ok)
{
	const expect_pos e = 0;

	EXPECT_TRUE(e.ok());
	EXPECT_EQ(e.val(), 0);
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), 0);
}

TEST(expect, expect_pos_const_error)
{
	const expect_pos e = std::errc::bad_address;

	EXPECT_FALSE(e.ok());
	EXPECT_DEATH(e.val(), "");
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_EQ(e.sc_rval(), -static_cast<long>(std::errc::bad_address));
}

TEST(expect, expect_ok)
{
	expect_ok e;

	EXPECT_TRUE(e.ok());
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), 0);

	e = std::errc::bad_address;
	EXPECT_FALSE(e.ok());
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_EQ(e.sc_rval(), -static_cast<long>(std::errc::bad_address));
}

TEST(expect, expect_ok_const_ok)
{
	const expect_ok e;

	EXPECT_TRUE(e.ok());
	EXPECT_DEATH(e.err(), "");
	EXPECT_EQ(e.sc_rval(), 0);
}

TEST(expect, expect_ok_const_error)
{
	const expect_ok e = std::errc::bad_address;

	EXPECT_FALSE(e.ok());
	EXPECT_EQ(e.err(), std::errc::bad_address);
	EXPECT_EQ(e.sc_rval(), -static_cast<long>(std::errc::bad_address));
}

TEST(expect, to_errc)
{
	EXPECT_EQ(to_errc(-EINVAL, std::errc::bad_address), std::errc::invalid_argument);
	EXPECT_EQ(to_errc(0, std::errc::bad_address), std::errc::bad_address);
	EXPECT_EQ(to_errc(1, std::errc::bad_address), std::errc::bad_address);
}
