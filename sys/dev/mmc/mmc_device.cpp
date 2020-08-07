#include "mmc_device.h"

#include "host.h"
#include "mmc_block.h"
#include <access.h>
#include <compiler.h>
#include <debug.h>
#include <dev/regulator/voltage/regulator.h>
#include <device.h>
#include <errno.h>
#include <fs/file.h>
#include <kernel.h>
#include <linux/major.h>
#include <linux/mmc/ioctl.h>
#include <string_utils.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

namespace mmc::mmc {

namespace {

int
rpmb_ioctl(file *f, unsigned long c, void *a)
{
	return reinterpret_cast<device *>(f->f_data)->ioctl(
						partition::rpmb, c, a);
}

const devio rpmb_io{
	.open = nullptr,
	.close = nullptr,
	.read = nullptr,
	.write = nullptr,
	.seek = nullptr,
	.ioctl = rpmb_ioctl,
};

}

/*
 * device::device
 */
device::device(host *h)
: ::mmc::device{h, mmc::tuning_cmd_index}
{ }

/*
 * device::~device
 */
device::~device()
{
	info("%s: MMC device %.*s%sdetached\n",
	    h_->name(), cid_.pnm().size(), cid_.pnm().data(),
	    *cid_.pnm().data() ? " " : "");

	if (rpmb_dev_) {
		/* hide device node */
		device_hide(rpmb_dev_);

		/* wait for active operations to complete */
		while (device_busy(rpmb_dev_))
			timer_delay(10e6);

		/* destroy device */
		device_destroy(rpmb_dev_);
	}
}

/*
 * device::init
 */
int
device::init()
{
	h_->assert_owned();

	cid_.clear();

	/* Get OCR by sending operating conditions with zero voltage window. */
	if (auto r = send_op_cond(h_, 0, ocr_); r < 0) {
		dbg("%s: MMC get OCR failed\n", h_->name());
		return r;
	}

	/* Switch to low voltage supply if card & host support it. */
	if (ocr_.v_170_195() && h_->vcc()->supports(1.70, 1.95) &&
	    h_->vio()->supports(1.70, 1.95) && h_->vcc()->get() > 1.95) {
		dbg("%s: MMC switching to 1.8V\n", h_->name());
		if (auto r = h_->power_cycle(1.8); r < 0)
			return r;
	}

	const auto supply = h_->vcc()->get();

	/* Check that device is compatible with our supply voltage. */
	if (!ocr_.supply_compatible(supply)) {
		info("%s: MMC device voltage incompatible\n", h_->name());
		return -ENOTSUP;
	}

	/* Initialise device. This can take up to 1 second. */
	for (const auto begin = timer_monotonic_coarse();;) {
		timer_delay(10e6);

		if (auto r = send_op_cond(h_, supply, ocr_); r < 0) {
			dbg("%s: MMC SEND_OP_COND failed\n", h_->name());
			return r;
		}

		const auto dt = timer_monotonic_coarse() - begin;

		if (!ocr_.busy()) {
			dbg("%s: MMC device took %ums to initialise\n",
			    h_->name(), (unsigned)(dt / 1e6));
			break;
		}

		if (dt > 1e9) {
			info("%s: MMC initialisation timeout\n", h_->name());
			return -ETIMEDOUT;
		}
	}

	if (auto r = all_send_cid(h_, cid_); r < 0) {
		dbg("%s: MMC ALL_SEND_CID failed\n", h_->name());
		return r;
	}

	if (auto r = set_relative_addr(h_, rca_); r < 0) {
		dbg("%s: MMC SET_RELATIVE_ADDR failed\n", h_->name());
		return r;
	}

	if (auto r = send_csd(h_, rca_, csd_); r < 0) {
		dbg("%s: MMC SEND_CSD failed\n", h_->name());
		return r;
	}

	if (csd_.csd_structure() < 2 || csd_.spec_vers() < 4) {
		info("%s: legacy MMC devices not supported\n", h_->name());
		return -ENOTSUP;
	}

	/* Note: we do not support unlocking devices. An error is returned here
	 * if the device is locked. */
	if (auto r = select_deselect_card(h_, rca_); r < 0) {
		dbg("%s: MMC SELECT/DESELECT_CARD failed\n", h_->name());
		return r;
	}

	if (auto r = send_ext_csd(h_, ext_csd_); r < 0) {
		dbg("%s: MMC SEND_EXT_CSD failed\n", h_->name());
		return r;
	}

	/* Determine maximum bus width. */
	h_->running_bus_test(true);
	unsigned bus_width = 1;
	for (unsigned w = 4; w <= h_->data_lines(); w *= 2) {
		h_->set_bus_width(w);
		if (bus_test(h_, rca_, w) == 0)
			bus_width = w;
	}
	h_->set_bus_width(1);
	h_->running_bus_test(false);

	/* True if vccq and vio are connected and separate from vcc. */
	const bool use_vccq = h_->vccq()->equal(h_->vio()) &&
	    !h_->vccq()->equal(h_->vcc());

	/* Host supports 1.2V i/o and vccq is connected to device. */
	const bool io_1v2 = use_vccq && h_->vio()->supports(1.1, 1.3);

	/* Host supports 1.8V i/o and vccq is connected to device, or device
	 * has already been switched to 1.8V VCC. */
	const bool running_1v8 = h_->vcc()->get() <= 1.95;
	const bool io_1v8 = (use_vccq && h_->vio()->supports(1.70, 1.95)) ||
	    running_1v8;

	/* DDR modes are only supported for 4- and 8-bit bus. */
	const bool ddr_ok = bus_width >= 4;

	/* Host & device support enhanced strobe. */
	const bool es_ok = ext_csd_.strobe_support() == 0x1 &&
	    h_->supports_enhanced_strobe();

	/* Determine ideal operating mode for device */
	unsigned long mode_rate;
	bool enh_strobe = false;
	auto try_mode = [&](device_type m, unsigned long max) {
		if (!h_->supports(m) || !ext_csd_.device_type().is_set(m))
			return false;
		mode_ = m;
		mode_rate = max;
		return true;
	};
	if (ddr_ok && io_1v2 && try_mode(device_type::hs400_1v2, 400e6))
		enh_strobe = es_ok;
	else if (ddr_ok && io_1v8 && try_mode(device_type::hs400_1v8, 400e6))
		enh_strobe = es_ok;
	else if (io_1v2 && try_mode(device_type::hs200_1v2, 200e6));
	else if (io_1v8 && try_mode(device_type::hs200_1v8, 200e6));
	else if (ddr_ok && io_1v2 && try_mode(device_type::ddr52_1v2, 104e6));
	else if (ddr_ok && try_mode(device_type::ddr52_1v8_3v3, 104e6));
	else if (try_mode(device_type::sdr52, 52e6));
	else if (try_mode(device_type::sdr26, 26e6));
	else {
		info("%s: MMC no compatible bus mode\n", h_->name());
		return -ENOTSUP;
	}

	/* Determine ideal drive strength & maximum data rate
	 * depending on total load capacitance & device capabilities. */
	unsigned long hw_rate = 0;
	driver_strength drive;
	auto try_drive = [&](driver_strength v) {
		if (!ext_csd_.driver_strength().is_set(v))
			return;
		const unsigned long max = h_->rate_limit(output_impedance(v));
		if (!hw_rate || max >= mode_rate) {
			hw_rate = max;
			drive = v;
		}
	};
	if (hs_mode(mode_)) {
		try_drive(driver_strength::type_1_33_ohm);
		try_drive(driver_strength::type_4_40_ohm);
		try_drive(driver_strength::type_0_50_ohm);
		try_drive(driver_strength::type_2_66_ohm);
		try_drive(driver_strength::type_3_100_ohm);
	} else
		try_drive(driver_strength::type_0_50_ohm);
	if (!hw_rate) {
		info("%s: MMC bad drive strength support\n", h_->name());
		return -ENOTSUP;
	}

	/* Maximum data rate is the minimum of what the hardware supports and
	 * the selected operating mode. */
	const bool ddr = ddr_mode(mode_);
	const unsigned long clk = std::min(hw_rate, mode_rate) / (ddr ? 2 : 1);

	/* Switch signalling voltage if necessary. */
	if (mode_ == device_type::hs400_1v2 ||
	    mode_ == device_type::hs200_1v2 ||
	    mode_ == device_type::ddr52_1v2) {
		dbg("%s: MMC switching to 1.2V signalling\n", h_->name());
		if (auto r = h_->set_vio(1.1, 1.3, 0); r < 0) {
			dbg("%s: MMC voltage switch failed\n", h_->name());
			return r;
		}
	} else if (!running_1v8 && (mode_ == device_type::hs400_1v8 ||
	    mode_ == device_type::hs200_1v8 ||
	    (io_1v8 && mode_ == device_type::ddr52_1v8_3v3))) {
		dbg("%s: MMC switching to 1.8V signalling\n", h_->name());
		if (auto r = h_->set_vio(1.70, 1.95, 0); r < 0) {
			dbg("%s: MMC voltage switch failed\n", h_->name());
			return r;
		}
	}

	/* REVISIT: For now we just set maximum power class and hope that the
	 * device accepts it. All tested devices don't seem to care. */
	if (auto r = ext_csd_.write(h_, rca_,
	    ext_csd::offset::power_class, 15); r < 0) {
		dbg("%s: MMC SWITCH POWER_CLASS failed\n", h_->name());
		return r;
	}

	/* Set drive strength & timing interface. */
	if (auto r = ext_csd_.write(h_, rca_,
	    ext_csd::offset::hs_timing,
	    static_cast<int>(drive) << 4 | timing_interface(mode_));
	    r < 0) {
		dbg("%s: MMC SWITCH HS_TIMING failed\n", h_->name());
		return r;
	}

	/* Switch bus width. */
	if (bus_width > 1) {
		dbg("%s: MMC switching to %u-bit bus%s\n", h_->name(),
		    bus_width, enh_strobe ? " with enhanced strobe" : "");
		if (auto r = ext_csd_.write(h_, rca_,
		    ext_csd::offset::bus_width,
		    enh_strobe << 7 | bus_mode(mode_, bus_width));
		    r < 0) {
			dbg("%s: SWITCH BUS_WIDTH failed\n", h_->name());
			return r;
		}
		h_->set_bus_width(bus_width);
	}

	/* Configure device clock. */
	const unsigned long devclk = h_->set_device_clock(clk,
	    ddr ? clock_mode::ddr : clock_mode::sdr, enh_strobe);
	dbg("%s: MMC clock %luMHz%s (requested %luMHz)\n", h_->name(),
	    devclk / 1000000, ddr ? " DDR" : " SDR", clk / 1000000);

	/* Check background operations handshake state. */
	if (ext_csd_.bkops_support() && ext_csd_.bkops_en() & 0x1)
	    warning("%s: WARNING: MMC MAN_BKOPS_EN handshake enabled. This is "
	        "NOT SUPPORTED.\n", h_->name());

	/* Calculate sector size. */
	if (ocr_.access_mode() == access_mode::byte) {
		sector_size_ = 1;
	} else {
		switch (ext_csd_.data_sector_size()) {
		case 0:
			sector_size_ = 512;
			break;
		case 1:
			sector_size_ = 4096;
			break;
		default:
			return DERR(-ENOTSUP);
		}
	}

	/* Enable cache. */
	if (ext_csd_.cache_size()) {
		dbg("%s: MMC switching on cache\n", h_->name());
		if (auto r = ext_csd_.write(h_, rca_,
		    ext_csd::offset::cache_ctrl, 0x1);
		    r < 0) {
			dbg("%s: SWITCH CACHE_CTRL failed\n", h_->name());
			return r;
		}
	}

	/* eMMC 6.2.5: ERASE_GROUP_DEF must be set to access partitions */
	if (auto r = ext_csd_.write(h_, rca_,
				    ext_csd::offset::erase_group_def, 0x1);
	    r < 0) {
		dbg("%s: SWITCH ERASE_GROUP_DEF failed\n", h_->name());
		return r;
	}

	/* Refresh ext_csd after changes. */
	if (auto r = send_ext_csd(h_, ext_csd_); r < 0) {
		dbg("%s: MMC SEND_EXT_CSD failed\n", h_->name());
		return r;
	}

	info("%s: MMC device %.*s attached in %s%s mode at address %u\n",
	    h_->name(), cid_.pnm().size(), cid_.pnm().data(), to_string(mode_),
	    enh_strobe ? " Enhanced Strobe" : "", rca_);

	info("%s: Hardware reset %s\n", h_->name(),
	    to_string(ext_csd_.rst_n_function()));
	info("%s: %luKiB cache, %s\n", h_->name(),
	    ext_csd_.cache_size() / 8, to_string(ext_csd_.cache_ctrl()));

	return add_partitions();
}

/*
 * device::read
 */
ssize_t
device::read(partition p, const iovec *iov, size_t iov_off, size_t len,
    off_t off)
{
	if (off % sector_size_ || len % sector_size_)
		return DERR(-EINVAL);

	std::lock_guard l{*h_};

	const size_t am = ocr_.access_mode() == access_mode::sector ? 512 : 1;

	if (auto r = switch_partition(p); r < 0)
		return r;

	size_t rd = 0;
	while (rd != len) {
		/* REVISIT: hard coded transfer block size of 512b for now. */
		/* see READ_BL_LEN and h->max_block_len */
		/* in DDR mode must be 512b */
		auto r = read_multiple_block(h_, iov, iov_off + rd, len - rd,
		    512, (off + rd) / am);
		if (r < 0)
			return r;
		if (r % sector_size_)
			return DERR(-EIO);
		rd += r;
	}

	return len;
}

/*
 * device::write
 */
ssize_t
device::write(partition p, const iovec *iov, size_t iov_off, size_t len,
    off_t off)
{
	if (off % sector_size_ || len % sector_size_)
		return DERR(-EINVAL);

	std::lock_guard l{*h_};

	const size_t am = ocr_.access_mode() == access_mode::sector ? 512 : 1;

	if (auto r = switch_partition(p); r < 0)
		return r;

	size_t wr = 0;
	while (wr != len) {
		/* REVISIT: hard coded transfer block size of 512b for now. */
		/* see WRITE_BL_LEN and h->max_block_len */
		/* in DDR mode must be 512b */
		auto r = write_multiple_block(h_, iov, iov_off + wr, len - wr,
		    512, (off + wr) / am, false);
		if (r < 0)
			return r;
		if (r % sector_size_)
			return DERR(-EIO);
		wr += r;
	}

	return len;
}

/*
 * device::ioctl
 */
int
device::ioctl(partition p, unsigned long cmd, void *arg)
{
	interruptible_lock ul(u_access_lock);
	if (auto r = ul.lock(); r < 0)
		return r;

	std::lock_guard hl{*h_};

	auto run_cmd = [this](mmc_ioc_cmd *c) {
		switch (c->opcode) {
		case 6: { /* switch */
			const auto arg = read_once(&c->arg);
			if (arg >> 24 != 3) /* write byte */
				return DERR(-ENOTSUP);
			return ext_csd_.write(h_, rca_,
			    static_cast<ext_csd::offset>(arg >> 16 & 0xff),
			    arg >> 8 & 0xff);
		}
		case 8: { /* send_ext_csd */
			if (c->write_flag || c->blksz != ext_csd_.size() ||
			    c->blocks != 1)
				return DERR(-EINVAL);
			void *p = reinterpret_cast<void *>(c->data_ptr);
			if (!u_access_ok(p, ext_csd_.size(), PROT_WRITE))
				return DERR(-EFAULT);
			if (auto r = send_ext_csd(h_, ext_csd_); r < 0)
				return r;
			memcpy(p, ext_csd_.data(), ext_csd_.size());
			return 0;
		}
		case 13: { /* send_status */
			device_status s;
			if (auto r = send_status(h_, rca_, s); r < 0)
				return r;
			c->response[0] = s.raw();
			return 0;
		}
		default:
			return DERR(-ENOTSUP);
		}
	};

	switch (cmd) {
	case MMC_IOC_CMD: {
		if (!ALIGNED(arg, mmc_ioc_cmd))
			return DERR(-EFAULT);
		if (!u_access_ok(arg, sizeof(mmc_ioc_cmd), PROT_WRITE))
			return DERR(-EFAULT);
		return run_cmd(static_cast<mmc_ioc_cmd *>(arg));
	}
	case MMC_IOC_MULTI_CMD: {
		if (!ALIGNED(arg, mmc_ioc_multi_cmd))
			return DERR(-EFAULT);
		if (!u_access_ok(arg, sizeof(mmc_ioc_multi_cmd), PROT_WRITE))
			return DERR(-EFAULT);
		mmc_ioc_multi_cmd *c = static_cast<mmc_ioc_multi_cmd *>(arg);
		if (!u_access_ok(c->cmds, sizeof(mmc_ioc_cmd) * c->num_of_cmds,
								PROT_WRITE))
			return DERR(-EFAULT);
		for (uint64_t i = 0; i != c->num_of_cmds; ++i)
			if (const auto r = run_cmd(c->cmds + i); r < 0)
				return r;
		return 0;
	}
	default:
		return DERR(-ENOSYS);
	}
}

/*
 * device::zeroout
 */
int
device::zeroout(partition p, off_t off, uint64_t len)
{
	std::unique_lock l{*h_};

	if (ext_csd_.erased_mem_cont() != 0 ||
	    !ext_csd_.sec_feature_support().is_set(sec_feature_support::sec_gb_cl_en))
		return -ENOTSUP;

	if (off % sector_size_ || len % sector_size_)
		return DERR(-EINVAL);

	if (auto r = switch_partition(p); r < 0)
		return r;

	/* trim one erase group at a time to allow for other i/o */
	return for_each_eg(off, len, [&](size_t start_lba, size_t end_lba) {
		l.unlock();
		l.lock();
		return trim(h_, start_lba, end_lba);
	});
}

/*
 * device::discard
 */
int
device::discard(partition p, off_t off, uint64_t len, bool secure)
{
	std::unique_lock l{*h_};

	/* TODO: support secure discard by using MMC secure trim? */
	if (secure)
		return -ENOTSUP;

	if (auto r = switch_partition(p); r < 0)
		return r;

	/* discard one erase group at a time to allow for other i/o */
	return for_each_eg(off, len, [&](size_t start_lba, size_t end_lba) {
		l.unlock();
		l.lock();
		return mmc::discard(h_, start_lba, end_lba);
	});
}

/*
 * device::discard_sets_to_zero
 */
bool
device::discard_sets_to_zero()
{
	std::unique_lock l{*h_};

	return ext_csd_.erased_mem_cont() == 0;
}

/*
 * device::v_mode
 */
device::mode_t
device::v_mode() const
{
	return mode_;
}

/*
 * for_each_eg
 *
 * Run operation for each erase group in range
 */
int
device::for_each_eg(off_t off, uint64_t len,
		    std::function<int(size_t, size_t)> fn)
{
	/* default to 4MiB if device doesn't report an erase group size */
	const auto eg_size = ext_csd_.hc_erase_grp_size() * 524288UL ?:
							4 * 1024 * 1024;

	/* do_op - run fn, adjust off & len */
	auto do_op = [&](uint64_t e) {
		auto s = std::min(len, e);
		if (auto r = fn(off / sector_size_,
				(off + s) / sector_size_ - 1); r < 0)
			return r;
		off += s;
		len -= s;

		return 0;
	};

	/* align operations with erase group */
	if (auto align = off % eg_size; align)
		if (auto r = do_op(eg_size - align); r < 0)
			return r;

	/* run across remaining erase groups */
	while (len)
		if (auto r = do_op(eg_size); r < 0)
			return r;

	return 0;
}


/*
 * device::switch_partition
 */
int
device::switch_partition(partition p)
{
	const unsigned config = ext_csd_.partition_config();
	if ((config & 7) == static_cast<unsigned>(p))
		return 0;

	return ext_csd_.write(h_, rca_, ext_csd::offset::partition_config,
	    (config & ~7) | static_cast<int>(p));
}

/*
 * device::add_partitions
 */
int
device::add_partitions()
{
	char b[32];
	::device *root, *dev;

	if (!(root = device_reserve("mmcblk", true)))
		return DERR(-ENOMEM);

	const auto usr_gp_scale = ext_csd_.hc_wp_grp_size() *
	    ext_csd_.hc_erase_grp_size() * 512 * 1024ull;
	const auto user_sz = ext_csd_.sec_count() * 512ull;
	const auto usr_enh = ext_csd_.partitions_attribute(partition::user);
	if (usr_enh) {
		const auto enh_sz = ext_csd_.enh_size_mult() * usr_gp_scale;
		const auto enh_start = ext_csd_.enh_start_addr() *
		    (ocr_.access_mode() == access_mode::sector ? 512ull : 1ull);
		info("%s: user partition %s %s (enhanced 0x%08llx -> 0x%08llx)\n",
		    h_->name(), root->name, hr_size_fmt(user_sz, b, 32),
		    enh_start, enh_start + enh_sz);
	} else
		info("%s: user partition %s %s\n", h_->name(),
		    root->name, hr_size_fmt(user_sz, b, 32));
	partitions_.emplace_back(this, root, partition::user, user_sz);

	if (ext_csd_.boot_size_mult()) {
		const auto sz = ext_csd_.boot_size_mult() * 128 * 1024ul;
		snprintf(b, 32, "%sboot", root->name);
		if (!(dev = device_reserve(b, true)))
			return DERR(-ENOMEM);
		info("%s: boot partition 1 %s %s\n", h_->name(),
		    dev->name, hr_size_fmt(sz, b, 32));
		partitions_.emplace_back(this, dev, partition::boot1, sz);
		snprintf(b, 32, "%sboot", root->name);
		if (!(dev = device_reserve(b, true)))
			return DERR(-ENOMEM);
		info("%s: boot partition 2 %s %s\n", h_->name(),
		    dev->name, hr_size_fmt(sz, b, 32));
		partitions_.emplace_back(this, dev, partition::boot2, sz);
	}

	for (auto p : {partition::gp1, partition::gp2, partition::gp3,
	    partition::gp4}) {
		const auto sz = ext_csd_.gp_size_mult_gpp(p) * usr_gp_scale;
		if (!sz)
			continue;
		snprintf(b, 32, "%sgp", root->name);
		if (!(dev = device_reserve(b, true)))
			return DERR(-ENOMEM);
		auto ext = ext_csd_.ext_partitions_attribute(p);
		info("%s: gp partition %d %s %s%s%s%s\n", h_->name(),
		    static_cast<int>(p) - 3, dev->name, hr_size_fmt(sz, b, 32),
		    ext_csd_.partitions_attribute(p) ? " (enhanced)" : "",
		    ext == ext_partitions_attribute::system_code ? " (system code)" : "",
		    ext == ext_partitions_attribute::non_persistent ? " (non-persistent)" : "");
		partitions_.emplace_back(this, dev, p, sz);
	}

	/* RPMB is not a block device, so don't treat it as such */
	if (ext_csd_.rpmb_size_mult()) {
		const auto sz = ext_csd_.rpmb_size_mult() * 128 * 1024ul;
		snprintf(b, 32, "%srpmb", root->name);
		if (!(rpmb_dev_ = device_create(&rpmb_io, b, DF_CHR, this)))
			return DERR(-ENOMEM);
		info("%s: rpmb partition %s %s\n", h_->name(),
		    dev->name, hr_size_fmt(sz, b, 32));
	};

	return 0;
}

}
