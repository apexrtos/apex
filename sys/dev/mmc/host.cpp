#include "host.h"

#include "command.h"
#include "device.h"
#include "mmc.h"
#include "mmc_device.h"
#include "sd.h"
#include "sd_card.h"
#include "sdio.h"
#include <arch.h>
#include <cerrno>
#include <debug.h>
#include <dev/regulator/voltage/regulator.h>
#include <sch.h>
#include <thread.h>

#include <kernel.h>
#include <page.h>

namespace mmc {

/*
 * host::host
 */
host::host(const mmc_desc &d, bool sdr104, bool sdr50, bool ddr50,
    bool hs400_es, bool hs400, bool hs200, bool ddr52, bool sdr52,
    bool sdr50_tuning, unsigned max_block_len)
: name_{d.name}
, removable_{d.removable}
, sdr104_{sdr104}
, sdr50_{sdr50}
, ddr50_{ddr50}
, hs400_es_{hs400_es}
, hs400_{hs400}
, hs200_{hs200}
, ddr52_{ddr52}
, sdr52_{sdr52}
, sdr50_tuning_{sdr50_tuning}
, enhanced_strobe_{false}
, tuning_enabled_{false}
, max_block_len_{max_block_len}
, power_stable_delay_ms_{d.power_stable_delay_ms}
, power_off_delay_ms_{d.power_off_delay_ms}
, data_lines_{d.data_lines}
, load_capacitance_pf_{d.load_capacitance_pf}
, max_rate_{d.max_rate}
, bus_changed_debounce_{}
, vcc_{regulator::voltage::bind(d.vcc_supply)}
, vio_{regulator::voltage::bind(d.vio_supply)}
, vccq_{regulator::voltage::bind(d.vccq_supply)}
{
	/* Configuration must specify vcc_supply, vio_supply and vccq_supply
	 * even if they are the same or fixed regulators. vccq must be equal to
	 * vcc or vio. Power stabilisation delay must be at least 1ms to meet
	 * SD spec. */
	if (!vcc_ || !vio_ || !vccq_ ||
	    (!vccq_->equal(vcc_) && !vccq_->equal(vio_)) ||
	    power_stable_delay_ms_ < 1 || load_capacitance_pf_ < 1)
		panic("bad desc");

	/* thread for rescanning bus after change events */
	if (!(th_ = kthread_create(&th_fn_wrapper, this, PRI_DPC, d.name,
	    MA_NORMAL)))
		panic("OOM");
}

/*
 * host::~host
 */
host::~host()
{ }

/*
 * host::lock - lock host for exclusive access
 */
void
host::lock()
{
	mutex_.lock();
}

/*
 * host::interruptible_lock - lock host for exclusive access if not signalled
 */
int
host::interruptible_lock()
{
	return mutex_.interruptible_lock();
}

/*
 * host::unlock - unlock host
 */
void
host::unlock()
{
	mutex_.unlock();
}

/*
 * host::assert_owned - assert that current thread has ownership of the host
 */
void
host::assert_owned() const
{
	mutex_.assert_locked();
}

/*
 * host::run_command
 */
ssize_t
host::run_command(command &c, unsigned rca)
{
	assert_owned();

	/* Tune bus if required. */
	if (tuning_enabled_ && v_require_tuning()) {
		dbg("%s: performing bus tuning\n", name());
		v_run_tuning(device_->tuning_cmd_index());
	}

	auto run_cmd = [&] {
		/* Application specific commands are prefixed by APP_CMD. */
		if (c.acmd()) {
			if (auto r = mmc::app_cmd(this, rca); r < 0)
				return r;
		}
		auto r = v_run_command(c);
		if (r < 0)
			return r;
		if (c.com_crc_error()) {
			dbg("%s: com_crc_error\n", name());
			return -EIO;
		}
		return r;
	};

	/* Retry command 3 times. */
	for (size_t i = 0; i < 2; ++i) {
		if (auto r = run_cmd(); r >= 0 || r == -EINTR)
			return r;

		/* Issue stop command to return to tran state. */
		if (c.data_size() > 0)
			mmc::stop_transmission(this);

		/* Commands fail in weird and wonderful ways if the bus isn't
		 * correctly tuned. Try to recover by tuning bus. */
		if (!device_)
			continue;
		if (!tuning_enabled_)
			continue;
		dbg("%s: tuning bus after command failure\n", name());
		if (auto r = v_run_tuning(device_->tuning_cmd_index()); r < 0)
			return r;
	}
	return run_cmd();
}

/*
 * host::rescan
 */
void
host::rescan()
{
	bus_changed_semaphore_.post_once();
}

/*
 * host::name
 */
const char *
host::name() const
{
	return name_.c_str();
}

/*
 * host::supports
 */
bool
host::supports(sd::access_mode mode) const
{
	switch (mode) {
	case sd::access_mode::default_sdr12:
	case sd::access_mode::high_sdr25:
		return true;
	case sd::access_mode::sdr50:
		return sdr50_;
	case sd::access_mode::sdr104:
		return sdr104_;
	case sd::access_mode::ddr50:
		return ddr50_;
	default:
		__builtin_unreachable();
	};
}

bool
host::supports(mmc::device_type mode) const
{
	switch (mode) {
	case mmc::device_type::sdr26:
	case mmc::device_type::sdr52:
		return true;
	case mmc::device_type::ddr52_1v8_3v3:
	case mmc::device_type::ddr52_1v2:
		return ddr52_;
	case mmc::device_type::hs200_1v8:
	case mmc::device_type::hs200_1v2:
		return hs200_;
	case mmc::device_type::hs400_1v8:
	case mmc::device_type::hs400_1v2:
		return hs400_;
	default:
		__builtin_unreachable();
	};
}

/*
 * host::supports_enhanced_strobe
 */
bool
host::supports_enhanced_strobe() const
{
	return hs400_es_;
}

/*
 * host::data_lines
 */
unsigned
host::data_lines() const
{
	return data_lines_;
}

/*
 * host::vcc
 */
const regulator::voltage *
host::vcc() const
{
	assert_owned();

	return vcc_;
}

/*
 * host::vio
 */
const regulator::voltage *
host::vio() const
{
	assert_owned();

	return vio_;
}

/*
 * host::vccq
 */
const regulator::voltage *
host::vccq() const
{
	assert_owned();

	return vccq_;
}

/*
 * host::rate_limit - estimate maximum clock/data rate for given bulk load
 *		      capacitance and driver output impedance
 */
unsigned long
host::rate_limit(unsigned output_impedance) const
{
	return std::min(max_rate_, (1000000000ul /
	    (64 * load_capacitance_pf_ * output_impedance)) * 10000);
}

/*
 * host::max_block_len - maximum block length supported by host
 */
unsigned
host::max_block_len() const
{
	return max_block_len_;
}

/*
 * host::power_cycle
 */
int
host::power_cycle(float nominal_voltage)
{
	assert_owned();

	/* Switch off power. */
	power_off();

	/* Wait for power supply to decay. */
	timer_delay(power_off_delay_ms_ * 1000000ull);

	/* Reset host controller */
	v_reset();

	/* Disable clock during initialisation. */
	v_disable_device_clock();
	v_set_bus_width(1);

	/* SD specification requires us to wait at least 1ms. */
	timer_delay(1e6);

	v_assert_hardware_reset();

	bool no_3v3_signalling = false;
	bool no_3v3_supply = nominal_voltage < 2.7;

	/* High voltage MMC & SD cards run from 2.7-3.6V. */
	if (no_3v3_supply || vcc_->set(2.7, 3.6) < 0) {
		/* Dual voltage MMC & eMMC can run from 1.65-1.95V. */
		if (vcc_->set(1.65, 1.95) < 0) {
			dbg("%s: failed to set initial vcc voltage\n", name());
			return DERR(-ENOTSUP);
		}

		no_3v3_signalling = true;
	}

	/* Set signalling voltage */
	if (no_3v3_signalling || vio_->set(2.6, 3.6) < 0) {
		/* For 1.8V signalling MMC specifies minimum voltage of 1.65V,
		 * but SD specifies a minimum of 1.70V. */
		if (vio_->set(1.70, 1.95) < 0) {
			/* eMMC can operate at 1.2V signalling. */
			if (vio_->set(1.1, 1.3) < 0) {
				vcc_->set(0.0, 0.0);
				dbg("%s: failed to set initial io voltage\n",
				    name());
				return DERR(-ENOTSUP);
			}
		}
	}

	/* Wait for power supplies to ramp up. */
	timer_delay(power_stable_delay_ms_ * 1000000ull);

	v_release_hardware_reset();

	/* Enable clock. */
	v_set_device_clock(400e3, clock_mode::sdr, false);
	v_enable_device_clock();

	/*
	 * SD: Wait for the longest of 1ms, 74 clocks or supply ramp time.
	 * MMC: Wait for 1ms, then 74 more clock cycles or supply ramp time.
	 *
	 * 2ms covers both cases.
	 */
	timer_delay(2e6);
	v_auto_device_clock();

	return 0;
}

/*
 * host::power_off
 */
void
host::power_off()
{
	assert_owned();

	v_disable_device_clock();
	vio_->set(0.0, 0.0);
	vcc_->set(0.0, 0.0);
}

/*
 * host::set_vio - set i/o voltage
 */
int
host::set_vio(float min_voltage, float max_voltage, unsigned delay_ms)
{
	assert_owned();
	assert(!vio_->equal(vcc_));

	if (auto r = vio_->set(min_voltage, max_voltage); r < 0)
		return r;

	timer_delay(std::max(power_stable_delay_ms_, delay_ms) * 1000000ull);

	return 0;
}

/*
 * host::set_bus_width
 */
void
host::set_bus_width(unsigned w)
{
	assert_owned();

	v_set_bus_width(w);
}

/*
 * host::device_busy
 */
bool
host::device_busy()
{
	assert_owned();

	return v_device_busy();
}

/*
 * host::disable_device_clock
 */
void
host::disable_device_clock()
{
	assert_owned();

	v_disable_device_clock();
}

/*
 * host::enable_device_clock
 */
void
host::enable_device_clock()
{
	assert_owned();

	v_enable_device_clock();
}

/*
 * host::auto_device_clock
 */
void
host::auto_device_clock()
{
	assert_owned();

	v_auto_device_clock();
}

/*
 * host::set_device_clock
 */
unsigned long
host::set_device_clock(unsigned long clock, clock_mode m, bool enhanced_strobe)
{
	assert_owned();

	enhanced_strobe_ = enhanced_strobe;
	return v_set_device_clock(clock, m, enhanced_strobe);
}

/*
 * host::running_bus_test - inform host that a bus test is running
 */
void
host::running_bus_test(bool v)
{
	assert_owned();

	return v_running_bus_test(v);
}

/*
 * host::bus_changed_irq - trigger rescan of bus
 *
 * Callable from irq context.
 */
void
host::bus_changed_irq()
{
	/* Debounce bus changed events by 200mS */
	timer_callout(&bus_changed_debounce_, 200e6, 0,
	    bus_changed_debounce_timeout, this);
}

/*
 * host::scan - scan the MMC/SD bus for inserted or removed devices
 */
void
host::scan()
{
	std::lock_guard l{mutex_};

	device_.reset();
	tuning_enabled_ = false;

	if (power_cycle(3.3) < 0)
		return;

	if (removable_ && !v_device_attached()) {
		power_off();
		return;
	}

	/* Attempt to initialise SD card before MMC device. The initialisation
	 * commands require this ordering. */
	int r = sd_initialise();
	if (r < 0)
		r = mmc_initialise();
	if (r == 0) {
		if (!bus_tuning_required())
			return;
		dbg("%s: performing initial bus tuning\n", name());
		v_enable_tuning();
		tuning_enabled_ = true;
		if (v_run_tuning(device_->tuning_cmd_index()) == 0)
			return;
		error("%s: initial bus tuning failed\n", name());
	}

	info("%s: failed to initialise device, retry in 1s\n", name());

	power_off();
	timer_delay(1e9);
	rescan();
}

/*
 * host::bus_tuning_required() - test if current access mode requires tuning
 */
bool
host::bus_tuning_required()
{
	assert_owned();
	assert(device_);

	return std::visit([&](auto v) {
		using T = std::decay_t<decltype(v)>;
		if constexpr (std::is_same_v<sd::access_mode, T>) {
			switch (v) {
			case sd::access_mode::default_sdr12:
			case sd::access_mode::high_sdr25:
				return false;
			case sd::access_mode::sdr50:
				return sdr50_tuning_;
			case sd::access_mode::sdr104:
			case sd::access_mode::ddr50:
				return true;
			}
		} else if constexpr(std::is_same_v<mmc::device_type, T>) {
			switch (v) {
			case mmc::device_type::sdr26:
			case mmc::device_type::sdr52:
			case mmc::device_type::ddr52_1v8_3v3:
			case mmc::device_type::ddr52_1v2:
				return false;
			case mmc::device_type::hs200_1v8:
			case mmc::device_type::hs200_1v2:
				return true;
			case mmc::device_type::hs400_1v8:
			case mmc::device_type::hs400_1v2:
				return !enhanced_strobe_;
			}
		} else
			static_assert(std::is_same_v<void, T>, "bad visitor");
		__builtin_unreachable();
	}, device_->mode());
}

/*
 * host::sd_initialise - attempt to initialise SD card
 *
 * Refer to:
 * - SD Host Controller Simplified Specification
 * - SDIO Simplified Specification
 *
 * We do not initialise SDIO cards or the SDIO portion of a combo card.
 * We do not support legacy SD cards.
 */
int
host::sd_initialise()
{
	assert_owned();

	/* SDIO or SD combo cards use CMD52 to reset the SDIO part. This is
	 * ignored by SD/eMMC devices. */
	sdio::reset(this);

	/* Reset card. */
	sd::go_idle_state(this);

	/*
	 * Perform voltage check (CMD8). This will fail:
	 * - for legacy cards.
	 * - for MMC and eMMC.
	 * - if IO voltage is inappropriate.
	 */
	if (auto r = sd::send_if_cond(this, vio_->get()); r < 0) {
		dbg("%s: SD SEND_IF_COND failed. MMC device?\n", name());
		return r;
	}

	/* Attempt to initialise card. */
	auto card = std::make_unique<sd::card>(this);
	if (auto r = card->init(); r < 0) {
		dbg("%s: SD card initialisation failed\n", name());
		return r;
	}

	device_ = std::move(card);

	return 0;
}

/*
 * mmc_initialise - attempt to initialise MMC device
 *
 * Refer to:
 *  - JEDEC Standard No. 84
 */
int
host::mmc_initialise()
{
	assert_owned();

	/* Reset device. */
	mmc::go_idle_state(this);

	/* Attempt to initialise device. */
	auto device = std::make_unique<mmc::device>(this);
	if (auto r = device->init(); r < 0) {
		dbg("%s: MMC device initialisation failed\n", name());
		return r;
	}

	device_ = std::move(device);

	return 0;
}

/*
 * host::th_fn
 */
void
host::th_fn()
{
	while (true) {
		bus_changed_semaphore_.wait_interruptible();
		scan();
	}
}

/*
 * host::th_fn_wrapper
 */
void
host::th_fn_wrapper(void *p)
{
	static_cast<host *>(p)->th_fn();
}

/*
 * host::bus_changed_debounce_timeout
 */
void
host::bus_changed_debounce_timeout(void *p)
{
	auto h = static_cast<host *>(p);
	h->bus_changed_semaphore_.post_once();
}

/*
 * host::add
 */
void
host::add(host *h)
{
	h->rescan();
}

}
