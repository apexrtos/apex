#include "imxrt10xx-ocotp.h"

#include "init.h"
#include <algorithm>
#include <arch.h>
#include <chrono>
#include <debug.h>
#include <errno.h>
#include <fs/util.h>
#include <kernel.h>
#include <timer.h>

#define trace(...)
namespace {

using namespace std::chrono_literals;

std::aligned_storage_t<sizeof(imxrt10xx::ocotp), alignof(imxrt10xx::ocotp)> mem;

constexpr auto unlock_key = 0x3e77;
constexpr auto read_locked_val = 0xbadabada;

/*
 * Wait Interval
 *	tSP_RD = (WAIT+1)/ipg_clk_freq
 */
constexpr auto tSP_RD = 150ns;

/*
 * Relax
 *	tSP_PGM = (RELAX+1)/ipg_clk_freq
 */
constexpr auto tSP_PGM = 100ns;

/*
 * Strobe Prog
 *	tPGM = ((STROBE_PROG+1)-2*(RELAX_PROG+1))/ipg_clk_freq
 */
constexpr auto tPGM = 10000ns;

/*
 * Strobe Read
 *	tRD = ((STROBE_READ+1)-2*(RELAX_READ+1))/ipg_clk_freq
 *	tAEN = (STROBE_READ+1)/ipg_clk_freq
 */
constexpr auto tRD = 45ns;
constexpr auto tAEN = 75ns;

/*
 * Relax Read
 *	tRRD = (RELAX_READ+1)/ipg_clk_freq
 */
constexpr auto tRRD = 10ns;

/*
 * Relax Prog
 *	tRPGM = (RELAX_PROG+1)/ipg_clk_freq
 */
constexpr auto tRPGM = 1000ns;

uint32_t
clocks(unsigned long clock, std::chrono::nanoseconds t)
{
	return div_ceil(clock * t.count(), std::nano::den);
}

}

namespace imxrt10xx {

/*
 * Registers
 */
struct ocotp::regs {
	union ctrl {
		struct {
			uint32_t ADDR : 6;
			uint32_t : 2;
			uint32_t BUSY : 1;
			uint32_t ERROR : 1;
			uint32_t RELOAD_SHADOWS : 1;
			uint32_t : 2;
			uint32_t : 3;
			uint32_t WR_UNLOCK : 16;
		};
		uint32_t r;
	} CTRL;
	ctrl CTRL_SET;
	ctrl CTRL_CLR;
	ctrl CTRL_TOG;
	union timing {
		struct {
			uint32_t STROBE_PROG : 12;
			uint32_t RELAX : 4;
			uint32_t STROBE_READ : 6;
			uint32_t WAIT : 6;
			uint32_t : 4;
		};
		uint32_t r;
	} TIMING;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t DATA;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	union read_ctrl {
		struct {
			uint32_t READ_FUSE : 1;
			uint32_t : 31;
		};
		uint32_t r;
	} READ_CTRL;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t READ_FUSE_DATA;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	union scs {
		struct {
			uint32_t HAB_JDE : 1;
			uint32_t : 30;
			uint32_t LOCK : 1;
		};
		uint32_t r;
	} SCS;
	scs SCS_SET;
	scs SCS_CLR;
	scs SCS_TOG;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	union version {
		struct {
			uint32_t STEP : 16;
			uint32_t MINOR : 8;
			uint32_t MAJOR : 8;
		};
		uint32_t r;
	} VERSION;
	uint32_t reserved[27];
	union timing2 {
		struct {
			uint32_t RELAX_PROG : 12;
			uint32_t : 4;
			uint32_t RELAX_READ : 6;
			uint32_t : 10;
		};
		uint32_t r;
	} TIMING2;
	uint32_t reserved1[191];
	struct otp {
		uint32_t BITS;
		uint32_t : 32;
		uint32_t : 32;
		uint32_t : 32;
	};
	otp OTP[80];

	static constexpr auto otp_word_sz = sizeof(otp::BITS);
};

/*
 * OCOTP Module
 */

ocotp::ocotp(const nxp_imxrt10xx_ocotp_desc *d)
: r_{reinterpret_cast<regs*>(d->base)}
{
	static_assert(sizeof(regs) == 0x900, "");
	static_assert(BYTE_ORDER == LITTLE_ENDIAN, "");

	uint32_t wait = clocks(d->clock, tSP_RD) - 1;
	uint32_t relax = clocks(d->clock, tSP_PGM) - 1;
	uint32_t relax_read = clocks(d->clock, tRRD) - 1;
	uint32_t relax_prog = clocks(d->clock, tRPGM) - 1;
	uint32_t prog = clocks(d->clock, tPGM) + 2 * (relax_prog + 1) - 1;
	uint32_t read = std::max(clocks(d->clock, tRD) + 2 * (relax_read + 1) - 1,
				 clocks(d->clock, tAEN) - 1);

	std::lock_guard l{mutex_};

	write32(&r_->TIMING, [&]{
		decltype(r_->TIMING) v{};
		v.WAIT = wait;
		v.STROBE_READ = read;
		v.RELAX = relax;
		v.STROBE_PROG = prog;
		return v.r;
	}());

	write32(&r_->TIMING2, [&]{
		decltype(r_->TIMING2) v{};
		v.RELAX_PROG = relax_prog;
		v.RELAX_READ = relax_read;
		return v.r;
	}());

	wait_busy();
}

ocotp *
ocotp::inst()
{
	return reinterpret_cast<ocotp *>(&mem);
}

void
ocotp::reload_shadow()
{
	check_and_clear_error();

	write32(&r_->CTRL_SET, [&]{
		auto v = read32(&r_->CTRL_SET);
		v.RELOAD_SHADOWS = 1;
		return v.r;
	}());

	wait_busy();
}

void
ocotp::wait_busy()
{
	mutex_.assert_locked();
	while (read32(&r_->CTRL).BUSY)
		timer_delay(0);
}

bool
ocotp::check_and_clear_error()
{
	if (!read32(&r_->CTRL).ERROR)
		return false;

	trace("OCOTP(%p) Error\n", r_);
	write32(&r_->CTRL_CLR, [&]{
		decltype(r_->CTRL_CLR) v{};
		v.ERROR = 1;
		return v.r;
	}());

	return true;
}

ssize_t
ocotp::read(std::span<std::byte> buf, off_t off)
{
	if (!data(buf))
		return DERR(-EFAULT);

	/* check if word aligned */
	if (size(buf) % regs::otp_word_sz || off % regs::otp_word_sz || off < 0)
		return DERR(-EINVAL);

	/* bytes are passed in - convert to words */
	off /= regs::otp_word_sz;
	auto words{size(buf) / regs::otp_word_sz};

	/* check if requested index is beyond the available */
	if (off >= std::size(r_->OTP))
		return 0;

	/* clip read size to maximum OTP registers */
	words = std::min(std::size(r_->OTP) - static_cast<size_t>(off), words);

	trace("ocotp::read index: %lld words: %d\n", off, words);

	std::lock_guard l{mutex_};
	for (auto i = 0u; i != words; ++i) {
		auto tmp = read32(&r_->OTP[i+off].BITS);
		static_assert(sizeof(tmp) == regs::otp_word_sz);
		memcpy(data(buf) + i * regs::otp_word_sz, &tmp, regs::otp_word_sz);
		if (tmp == read_locked_val) {
			check_and_clear_error();
			trace("OCOTP(%p) Failed to read locked region: %u\n",
				r_, i);
		}
	}
	return words * regs::otp_word_sz;
}

ssize_t
ocotp::write(std::span<const std::byte> buf, off_t off)
{
	if (!data(buf))
		return DERR(-EFAULT);

	/* check if word aligned */
	if (size(buf) % regs::otp_word_sz || off % regs::otp_word_sz || off < 0)
		return DERR(-EINVAL);

	/* bytes are passed in - convert to words */
	off /= regs::otp_word_sz;
	auto words{size(buf) / regs::otp_word_sz};

	/* check if requested index is beyond the available */
	if (off >= std::size(r_->OTP))
		return 0;

	/* clip read size to maximum OTP registers */
	words = std::min(std::size(r_->OTP) - static_cast<size_t>(off), words);

	trace("ocotp::write index: %lld words: %d\n", off, words);

	std::lock_guard l{mutex_};
	for (auto i = 0u; i != words; ++i) {
		/*
		 * i.MX RT1060 reference manual as at
		 * Rev. 2, 12/2019 23.6.1.35 Value of OTP Bank7 Word0 (GP3) (GP30)
		 * The address index for the registers at and above 0x880h
		 * (72) become non linear. We therefore must remove a value
		 * of 16 to re-align.
		 */
		auto addr = i + off;
		if (addr >= 72)
			addr -= 16;

		uint32_t tmp;
		static_assert(sizeof(tmp) == regs::otp_word_sz);
		memcpy(&tmp, data(buf) + i * regs::otp_word_sz, regs::otp_word_sz);

		trace("index: %llu data: 0x%08x\n", addr, tmp);

		write32(&r_->CTRL, [&]{
			decltype(r_->CTRL) v{};
			v.ADDR = addr;
			v.WR_UNLOCK = unlock_key;
			return v.r;
		}());

		write32(&r_->DATA, tmp);

		wait_busy();
		if (check_and_clear_error()) {
			trace("OCOTP(%p) Failed to write locked region: %llu\n",
				r_, addr);
			return DERR(-EPERM);
		}

		/*
		* i.MX RT1060 reference manual as at
		* Rev. 2, 12/2019 23.4.1.4 Write Postamble
		*/
		timer_delay(2e3);
	}

	reload_shadow();
	return words * regs::otp_word_sz;
}

}

ssize_t
ocotp_read_iov(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(f, iov, count, offset,
	    [](std::span<std::byte> buf, off_t offset) {
		return imxrt10xx::ocotp::inst()->read(buf, offset);
	});
}

ssize_t
ocotp_write_iov(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(f, iov, count, offset,
	    [](std::span<const std::byte> buf, off_t offset) {
		return imxrt10xx::ocotp::inst()->write(buf, offset);
	});
}

/*
 * nxp_imxrt10xx_ocotp_init
 */
extern "C" void
nxp_imxrt10xx_ocotp_init(const nxp_imxrt10xx_ocotp_desc *d)
{
	new(&mem) imxrt10xx::ocotp{d};

#if defined(CONFIG_DEBUG)
	const auto v = read32(&imxrt10xx::ocotp::inst()->r_->VERSION);
	dbg("OCOTP %d.%d.%d initialised\n", v.MAJOR, v.MINOR, v.STEP);
#endif

	static constinit devio io{
		.read = ocotp_read_iov,
		.write = ocotp_write_iov,
	};

	if (!device_create(&io, d->name, DF_BLK, &mem))
		DERR(-EINVAL);
}
