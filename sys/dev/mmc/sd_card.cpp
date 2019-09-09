#include "sd_card.h"

#include "host.h"
#include "sd_block.h"
#include <debug.h>
#include <dev/regulator/voltage/regulator.h>
#include <device.h>
#include <errno.h>
#include <string_utils.h>

namespace mmc::sd {

/*
 * card::card
 */
card::card(host *h)
: device{h, sd::tuning_cmd_index}
{ }

/*
 * card::~card
 */
card::~card()
{
	info("%s: SD card %.*s detached\n",
	    h_->name(), cid_.pnm().size(), cid_.pnm().data());
}

/*
 * card::init
 */
int
card::init()
{
	h_->assert_owned();

	const auto supply = h_->vcc()->get();

	/* Do not send CMD5. This will leave SDIO or the IO part of combo cards
	 * uninitialised & unresponsive. */

	/* Get OCR by sending operating conditions with zero voltage window. */
	if (auto r = sd_send_op_cond(h_, false, 0, ocr_); r < 0) {
		dbg("%s: SD get OCR failed\n", h_->name());
		return r;
	}

	/* Check that card is compatible with our supply voltage. */
	if (!ocr_.supply_compatible(supply)) {
		info("%s: SD card voltage incompatible\n", h_->name());
		return -ENOTSUP;
	}

	/* True if vio is controlled by a separate regulator. */
	const bool use_vio = !h_->vio()->equal(h_->vcc());

	/* Request switch to 1.8V if we can run 1.8V signalling. */
	const bool s18r = use_vio && h_->vio()->supports(1.70, 1.95);

	/* Initialise card. This can take up to 1 second. */
	for (const auto begin = timer_monotonic_coarse();;) {
		timer_delay(10e6);

		if (auto r = sd_send_op_cond(h_, s18r, supply, ocr_); r < 0) {
			dbg("%s: SD SD_SEND_OP_COND failed\n", h_->name());
			return r;
		}

		const auto dt = timer_monotonic_coarse() - begin;

		if (!ocr_.busy()) {
			dbg("%s: SD card took %ums to initialise\n",
			    h_->name(), (unsigned)(dt / 1e6));
			break;
		}

		if (dt > 1e9) {
			info("%s: SD initialisation timeout\n", h_->name());
			return -ETIMEDOUT;
		}
	}

	/* CCS and S18A set means that the host & card are UHS-I compatible. */
	const bool uhsi = ocr_.ccs() && ocr_.s18a();

	/* Switch to UHS-I 1.8V signalling & SDR12 timing. */
	if (uhsi) {
		dbg("%s: SD switching to 1.8V signalling\n", h_->name());

		/* Card has just been initialised. By definition it cannot be
		 * busy yet. */
		if (h_->device_busy()) {
			dbg("%s: SD can't switch voltage on busy card\n",
			    h_->name());
			return DERR(-EIO);
		}

		if (auto r = voltage_switch(h_); r < 0) {
			dbg("%s: SD voltage switch command failed\n",
			    h_->name());
			return r;
		}

		/* Card drives DAT[3:0] low after receiving CMD11. */
		if (!h_->device_busy()) {
			dbg("%s: SD card did not start voltage switch\n",
			    h_->name());
			return DERR(-EIO);
		}

		h_->disable_device_clock();

		/* Switch host i/o voltage. Clock must be gated for at least
		 * 5ms. */
		if (auto r = h_->set_vio(1.70, 1.95, 5); r < 0) {
			dbg("%s: SD host voltage switch failed\n",
			    h_->name());
			return r;
		}

		/* Clock the card for at least 1ms. */
		h_->set_device_clock(25e6, clock_mode::sdr, false);
		h_->enable_device_clock();
		timer_delay(1e6);
		h_->auto_device_clock();

		/* Card signals success by releasing DAT[3:0]. */
		if (h_->device_busy()) {
			dbg("%s: SD card did not complete voltage switch\n",
			    h_->name());
			return DERR(-EIO);
		}
	}

	if (auto r = all_send_cid(h_, cid_); r < 0) {
		dbg("%s: SD ALL_SEND_CID failed\n", h_->name());
		return r;
	}

	if (auto r = send_relative_addr(h_, rca_); r < 0) {
		dbg("%s: SD SEND_RELATIVE_ADDR failed\n", h_->name());
		return r;
	}

	if (auto r = send_csd(h_, rca_, csd_); r < 0) {
		dbg("%s: SD SEND_CSD failed\n", h_->name());
		return r;
	}

	if (csd_.csd_structure() != 1) {
		info("%s: SD CSD version not supported\n", h_->name());
		return -ENOTSUP;
	}

	/* Note: we do not support unlocking cards. An error is returned here
	 * if the card is locked. */
	if (auto r = select_deselect_card(h_, rca_); r < 0) {
		dbg("%s: SD SELECT/DESELECT_CARD failed\n", h_->name());
		return r;
	}

	/* UHS-I cards are only required to support data line command CMD42
	 * (LOCK_UNLOCK) in 1-bit bus mode, so SEND_SCR can fail. In that case
	 * we switch to 4-bit mode if we can and try again. */
	const bool scr_ok = send_scr(h_, rca_, scr_) == 0;

	if (scr_ok && scr_.scr_structure() != 0) {
		info("%s: SD SCR version not supported\n", h_->name());
		return -ENOTSUP;
	}

	if (h_->data_lines() >= 4 &&
	    (uhsi || !scr_ok || scr_.sd_bus_widths() & 0x4)) {
		info("%s: SD switching to 4-bit bus\n", h_->name());
		if (auto r = set_bus_width(h_, rca_, 4); r < 0) {
			dbg("%s: SD SET_BUS_WIDTH failed\n", h_->name());
			return r;
		}
		h_->set_bus_width(4);
	}

	/* SEND_SCR will now succeed for all usable cards. */
	if (auto r = send_scr(h_, rca_, scr_); r < 0) {
		info("%s: SD incompatible host/card combination?\n",
		    h_->name());
		return r;
	}

	if (scr_.scr_structure() != 0) {
		info("%s: SD SCR version not supported\n", h_->name());
		return -ENOTSUP;
	}

	/* We require at least version 1.10 to support SWITCH_FUNC command. */
	if (scr_.sd_spec() == 0) {
		info("%s: SD version 1.01 cards not supported\n", h_->name());
		return -ENOTSUP;
	}

	function_status fs;
	if (auto r = check_func(h_, fs); r < 0) {
		dbg("%s: SD CHECK_FUNC failed\n", h_->name());
		return r;
	}

	/* Determine ideal operating mode for card. */
	unsigned long mode_rate;
	auto try_mode = [&](access_mode m, unsigned long max) {
		if (!h_->supports(m) || !fs.access_mode().is_set(m))
			return false;
		mode_ = m;
		mode_rate = max;
		return true;
	};
	if (uhsi && try_mode(access_mode::sdr104, 208e6));
	else if (uhsi && try_mode(access_mode::ddr50, 100e6));
	else if (uhsi && try_mode(access_mode::sdr50, 100e6));
	else if (try_mode(access_mode::high_sdr25, 50e6));
	else if (try_mode(access_mode::default_sdr12, 25e6));
	else {
		info("%s: SD no compatible bus mode\n", h_->name());
		return -ENOTSUP;
	}

	/* Determine ideal drive strength & maximum data rate
	 * depending on total load capacitance & card capabilities. */
	unsigned long hw_rate = 0;
	driver_strength drive;
	auto try_drive = [&](driver_strength v) {
		if (!fs.driver_strength().is_set(v))
			return;
		const unsigned long max = h_->rate_limit(output_impedance(v));
		if (!hw_rate || max >= mode_rate) {
			hw_rate = max;
			drive = v;
		}
	};
	if (uhsi) {
		try_drive(driver_strength::type_a_33_ohm);
		try_drive(driver_strength::type_b_50_ohm);
		try_drive(driver_strength::type_c_66_ohm);
		try_drive(driver_strength::type_d_100_ohm);
	} else
		try_drive(driver_strength::type_b_50_ohm);
	if (!hw_rate) {
		info("%s: SD bad function support\n", h_->name());
		return -ENOTSUP;
	}

	/* Maximum data rate is the minimum of what the hardware supports and
	 * the selected operating mode. */
	const bool ddr = ddr_mode(mode_);
	const unsigned long clk = std::min(hw_rate, mode_rate) / (ddr ? 2 : 1);

	/* Determine maximum power limit. */
	power_limit power = power_limit::w_0_72;
	if (fs.power_limit().is_set(power_limit::w_2_88))
		power = power_limit::w_2_88;
	else if (fs.power_limit().is_set(power_limit::w_2_16))
		power = power_limit::w_2_16;
	else if (fs.power_limit().is_set(power_limit::w_1_44))
		power = power_limit::w_1_44;

	/* Set card drive strength, operating mode & power limit. */
	if (auto r = switch_func(h_, power, drive, mode_);
	    r < 0) {
		dbg("%s: SD SWITCH_FUNC failed\n", h_->name());
		return r;
	}

	/* Configure card clock. */
	const unsigned long devclk = h_->set_device_clock(clk,
	    ddr ? clock_mode::ddr : clock_mode::sdr, false);
	dbg("%s: SD clock %luMHz%s (requested %luMHz)\n", h_->name(),
	    devclk / 1000000, ddr ? " DDR" : " SDR", clk / 1000000);

	/* Calculate sector size. */
	sector_size_ = ocr_.ccs() ? 512 : 1;

	info("%s: SD card %.*s attached in %s mode with RCA %04x\n",
	    h_->name(), cid_.pnm().size(), cid_.pnm().data(), to_string(mode_),
	    rca_);

	/* Create block device. */
	::device *dev;
	if (!(dev = device_reserve("mmcblk", true)))
		return DERR(-ENOMEM);
	const auto size = csd_.c_size() * 524288ULL;
	block_ = std::make_unique<block>(this, dev, size);

	char b[32];
	info("%s: %s %s\n", h_->name(), dev->name, hr_size_fmt(size, b, 32));

	return 0;
}

/*
 * card::read
 */
ssize_t
card::read(const iovec *iov, size_t iov_off, size_t len, off_t off)
{
	if (off % sector_size_ || len % sector_size_)
		return DERR(-EINVAL);

	std::lock_guard l{*h_};

	size_t rd = 0;
	while (rd != len) {
		/* SD cards always use 512b transfer block size. */
		auto r = read_multiple_block(h_, iov, iov_off + rd, len - rd,
		    512, (off + rd) / sector_size_);
		if (r < 0)
			return r;
		if (r % sector_size_)
			return DERR(-EIO);
		rd += r;
	}

	return len;
}

/*
 * card::write
 */
ssize_t
card::write(const iovec *iov, size_t iov_off, size_t len, off_t off)
{
	if (off % sector_size_ || len % sector_size_)
		return DERR(-EINVAL);

	std::lock_guard l{*h_};

	size_t wr = 0;
	while (wr != len) {
		/* SD cards always use 512b transfer block size. */
		auto r = write_multiple_block(h_, iov, iov_off + wr, len - wr,
		    512, (off + wr) / sector_size_);
		if (r < 0)
			return r;
		if (r % sector_size_)
			return DERR(-EIO);
		wr += r;
	}

	return len;
}

/*
 * card::ioctl
 */
int
card::ioctl(unsigned long cmd, void *arg)
{
	return DERR(-ENOSYS);
}

/*
 * card::v_mode
 */
card::mode_t
card::v_mode() const
{
	return mode_;
}

}
