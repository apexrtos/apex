#ifndef dev_mmc_host_h
#define dev_mmc_host_h

/*
 * Generic MMC/SD Host Controller
 */

#include "desc.h"
#include <string>
#include <sync.h>
#include <timer.h>

struct thread;
namespace regulator { class voltage; }

namespace mmc {

class device;
class command;
namespace sd { enum class access_mode; }
namespace mmc { enum class device_type; }

enum class clock_mode {
	sdr,
	ddr,
};

class host {
public:
	host(const mmc_desc &, bool sdr104, bool ddr50, bool sdr50,
	    bool hs400_es, bool hs400, bool hs200, bool ddr52, bool sdr52,
	    bool sdr50_tuning, unsigned max_block_len);
	virtual ~host();

	void lock();
	void unlock();
	void assert_owned() const;

	ssize_t run_command(command &, unsigned rca);
	void rescan();
	const char *name() const;

	bool supports(sd::access_mode) const;
	bool supports(mmc::device_type) const;
	bool supports_enhanced_strobe() const;
	unsigned data_lines() const;
	const regulator::voltage *vcc() const;
	const regulator::voltage *vio() const;
	const regulator::voltage *vccq() const;
	unsigned long rate_limit(unsigned output_impedance) const;
	unsigned max_block_len() const;

	int power_cycle(float nominal_voltage);
	void power_off();
	int set_vio(float min_voltage, float max_voltage, unsigned delay_ms);
	void set_bus_width(unsigned);
	bool device_busy();
	void disable_device_clock();
	void enable_device_clock();
	void auto_device_clock();
	unsigned long set_device_clock(unsigned long clock, clock_mode,
	    bool enhanced_strobe);
	void running_bus_test(bool);

protected:
	void bus_changed_irq();

private:
	virtual void v_reset() = 0;
	virtual void v_disable_device_clock() = 0;
	virtual unsigned long v_set_device_clock(unsigned long, clock_mode,
	    bool enhanced_strobe) = 0;
	virtual void v_enable_device_clock() = 0;
	virtual void v_auto_device_clock() = 0;
	virtual void v_assert_hardware_reset() = 0;
	virtual void v_release_hardware_reset() = 0;
	virtual ssize_t v_run_command(command &) = 0;
	virtual bool v_device_attached() = 0;
	virtual bool v_device_busy() = 0;
	virtual void v_set_bus_width(unsigned) = 0;
	virtual void v_enable_tuning() = 0;
	virtual bool v_require_tuning() = 0;
	virtual int v_run_tuning(unsigned cmd_index) = 0;
	virtual void v_running_bus_test(bool) = 0;

	const std::string name_;
	const bool removable_ : 1;	/* device is removable */
	const bool sdr104_ : 1;		/* SD card modes. */
	const bool sdr50_ : 1;
	const bool ddr50_ : 1;
	const bool hs400_es_ : 1;	/* eMMC modes. */
	const bool hs400_ : 1;
	const bool hs200_ : 1;
	const bool ddr52_ : 1;
	const bool sdr52_ : 1;
	const bool sdr50_tuning_ : 1;	/* SDR50 requires tuning. */
	bool enhanced_strobe_ : 1;
	bool tuning_enabled_ : 1;
	const unsigned max_block_len_;
	const unsigned power_stable_delay_ms_;
	const unsigned power_off_delay_ms_;
	const unsigned data_lines_;
	const unsigned load_capacitance_pf_;
	const unsigned long max_rate_;

	a::mutex mutex_;
	thread *th_;
	timer bus_changed_debounce_;
	a::semaphore bus_changed_semaphore_;
	regulator::voltage *vcc_;
	regulator::voltage *vio_;
	regulator::voltage *vccq_;
	std::unique_ptr<device> device_;

	void scan();
	bool bus_tuning_required();
	int sd_initialise();
	int mmc_initialise();

	void th_fn();
	static void th_fn_wrapper(void *);
	static void bus_changed_debounce_timeout(void *);

public:
	static void add(host *);
};

}

#endif
