#include "imxrt10xx-rtwdog.h"

#include "init.h"
#include <arch/mmio.h>
#include <debug.h>
#include <device.h>
#include <fs/util.h>
#include <kernel.h>
#include <linux/watchdog.h>

#define trace(...)

namespace {

std::aligned_storage_t<sizeof(imxrt10xx::rtwdog), alignof(imxrt10xx::rtwdog)> mem;

constexpr auto unlock_key{0xd928c520};
constexpr auto refresh_key{0xb480a602};

}

namespace imxrt10xx {

/*
 * Registers
 */
struct rtwdog::regs {
	union cs {
		struct {
			uint32_t STOP : 1;
			uint32_t WAIT : 1;
			uint32_t DBG : 1;
			uint32_t TST : 2;
			uint32_t UPDATE : 1;
			uint32_t INT : 1;
			uint32_t EN : 1;
			uint32_t CLK : 2;
			uint32_t RCS : 1;
			uint32_t ULK : 1;
			uint32_t PRES : 1;
			uint32_t CMD32EN : 1;
			uint32_t FLG : 1;
			uint32_t WIN : 1;
			uint32_t : 16;
		};
		uint32_t r;
	} CS;
	union cnt {
		struct {
			uint32_t LOW : 8;
			uint32_t HIGH : 8;
			uint32_t : 16;
		};
		uint32_t r;
	} CNT;
	cnt TOVAL;
	cnt WIN;
};

rtwdog::rtwdog(const nxp_imxrt10xx_rtwdog_desc *d)
: r_{reinterpret_cast<regs*>(d->base)}
, clock_{d->prescale_256 ? div_ceil(d->freq, 256lu) : d->freq}
, open_{false}
, expect_close_{false}
{
	static_assert(sizeof(regs) == 0x10);
	static_assert(BYTE_ORDER == LITTLE_ENDIAN);

	configure(d->clock, d->prescale_256);
	if (set_timeout(d->default_timeout) < 0)
		panic("bad timeout");
}

rtwdog *
rtwdog::inst()
{
	return reinterpret_cast<rtwdog *>(&mem);
}

void
rtwdog::configure(clock clock, bool prescale_256)
{
	std::lock_guard l{lock_};

	/* unlock and configure within 255 bus clocks */
	write32(&r_->CNT, unlock_key);
	while (!read32(&r_->CS).ULK);

	/*
	 * CS is allowed to be written once after 255 clock cycles after reset
	 * unless UPDATE = 1.
	 * Bootrom writes UPDATE = 1 shortly after reset. This allows this
	 * driver to update the value of CS.
	 */
	write32(&r_->CS, decltype(r_->CS){{
		.STOP{0},
		.WAIT{0},
		.DBG{0},
		.TST{0},
		.UPDATE{1},
		.INT{0},
		.EN{0}, /* keep off until open device is opened */
		.CLK{static_cast<unsigned>(clock)},
		.PRES{prescale_256},
		.CMD32EN{1},
		.WIN{0},
	}}.r);
	while (!read32(&r_->CS).RCS);
}

void
rtwdog::enable(bool en)
{
	trace("imxrt10xx::rtwdog::enable: %d\n", en);

	std::lock_guard l{lock_};
	/* unlock and enable timer within 255 bus clocks */
	write32(&r_->CNT, unlock_key);
	while (!read32(&r_->CS).ULK);
	write32(&r_->CS, [&]{
		auto v = read32(&r_->CS);
		v.EN = en;
		return v.r;
	}());
	while (!read32(&r_->CS).RCS);
}

int
rtwdog::set_timeout(unsigned t)
{
	trace("imxrt10xx::rtwdog::set_timeout: %ds\n", t);

	/* sanity check requested timeout */
	if (t < 1 || t > UINT16_MAX / clock_)
		return DERR(-ERANGE);

	std::lock_guard l{lock_};
	write32(&r_->CNT, unlock_key);
	while (!read32(&r_->CS).ULK);
	write32(&r_->TOVAL, t * clock_);
	while (!read32(&r_->CS).RCS);

	return 0;
}

int
rtwdog::get_timeout()
{
	trace("imxrt10xx::rtwdog::get_timeout\n");
	const auto v{read32(&r_->TOVAL).r};
	return (v + 1) / clock_;
}

int
rtwdog::get_timeleft()
{
	trace("imxrt10xx::rtwdog::get_timeleft\n");

	/*
	 * CNT counts up.
	 * Subtract count(CNT) from timeout value(TOVAL) to get time left.
	 */
	std::lock_guard l{lock_};
	const auto c{read32(&r_->CNT).r};
	const auto t{read32(&r_->TOVAL).r};
	return (t - c + 1) / clock_;
}

void
rtwdog::refresh()
{
	trace("imxrt10xx::rtwdog::refresh\n");

	/*
	 * One 32bit write of the key to the CNT register when
	 * CMD32EN = 1 to reset the CNT value to 0
	 */
	write32(&r_->CNT, refresh_key);
}

int
rtwdog::open()
{
	trace("imxrt10xx::rtwdog::open\n");

	bool v{false};
	if (!open_.compare_exchange_strong(v, true))
		return DERR(-EBUSY);

	enable(true);
	return 0;
}

int
rtwdog::close()
{
	trace("imxrt10xx::rtwdog::close\n");
	if (expect_close_)
		enable(false);

	expect_close_ = false;
	open_ = false;
	return 0;
}

int
rtwdog::ioctl(unsigned long cmd, void *arg)
{
	switch(cmd) {
		case WDIOC_KEEPALIVE: {
			refresh();
			return 0;
		}
		case WDIOC_SETTIMEOUT: {
			int to;
			memcpy(&to, arg, sizeof(to));
			/*
			 * Reset the counter so any new value that is less than
			 * the current counter value wont cause a reset
			 */
			refresh();
			return set_timeout(to);
		}
		case WDIOC_GETTIMEOUT: {
			const int to{get_timeout()};
			memcpy(arg, &to, sizeof(to));
			return 0;
		}
		case WDIOC_GETTIMELEFT: {
			const int tl{get_timeleft()};
			memcpy(arg, &tl, sizeof(tl));
			return 0;
		}
	}

	return DERR(-ENOSYS);
}

ssize_t
rtwdog::write(std::span<const std::byte> buf, off_t off)
{
	trace("imxrt10xx::rtwdog::write\n");

	/* scan for magic 'V' to disable watchdog on close */
	const auto magic{static_cast<std::byte>('V')};
	expect_close_ = std::find(begin(buf), end(buf), magic) != end(buf);

	refresh();
	return size(buf);
}

}

int
rtwdog_open(file *f)
{
	return imxrt10xx::rtwdog::inst()->open();
}

int
rtwdog_close(file *f)
{
	return imxrt10xx::rtwdog::inst()->close();
}

ssize_t
rtwdog_write(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [](std::span<const std::byte> buf, off_t offset) {
		return imxrt10xx::rtwdog::inst()->write(buf, offset);
	});
}

int
rtwdog_ioctl(file *f, unsigned long cmd, void *arg)
{
	return imxrt10xx::rtwdog::inst()->ioctl(cmd, arg);
}

/*
 * nxp_imxrt10xx_rtwdog_init
 */
void
nxp_imxrt10xx_rtwdog_init(const nxp_imxrt10xx_rtwdog_desc *d)
{
	dbg("imxrt10xx::rtwdog(%#lx): init\n", d->base);

	new(&mem) imxrt10xx::rtwdog{d};

	static constinit devio io{
		.open = rtwdog_open,
		.close = rtwdog_close,
		.write = rtwdog_write,
		.ioctl = rtwdog_ioctl,
	};

	device_create(&io, d->name, DF_CHR, nullptr);
}
