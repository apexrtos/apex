#ifndef dev_mmc_sd_h
#define dev_mmc_sd_h

/*
 * SD support
 */

#include "bitfield.h"
#include <array>
#include <cstdint>
#include <string>

struct iovec;
namespace mmc { class host; }

namespace mmc::sd {

/*
 * SD Operating Conditions Register
 */
class ocr final {
public:
	ocr();
	ocr(void *);

	bool v_270_280() const;
	bool v_280_290() const;
	bool v_290_300() const;
	bool v_300_310() const;
	bool v_310_320() const;
	bool v_320_330() const;
	bool v_330_340() const;
	bool v_340_350() const;
	bool v_350_360() const;
	bool s18a() const;		    /* UHS-I cards only */
	bool uhs_ii_status() const;
	bool ccs() const;		    /* only valid if busy == 1 */
	bool busy() const;

	bool supply_compatible(float supply_v) const;

private:
	uint32_t r_;
};

/*
 * SD Card Identification Register
 */
class cid final {
public:
	cid();
	cid(void *);

	void clear();

	unsigned mid() const;
	unsigned oid() const;
	std::string_view pnm() const;
	unsigned prv() const;
	unsigned long psn() const;
	unsigned mdt() const;

private:
	alignas(4) std::array<std::byte, 16> r_;
};

/*
 * SD Card Specific Data Register
 */
class csd final {
public:
	csd();
	csd(void *);

	unsigned csd_structure() const;
	unsigned ccc() const;
	bool dsr_imp() const;
	unsigned long c_size() const;
	bool copy() const;
	bool perm_write_protect() const;
	bool tmp_write_protect() const;

private:
	alignas(4) std::array<std::byte, 16> r_;
};

/*
 * SD Card Configuration Register
 */
class scr final {
public:
	scr();

	void *data();
	size_t size() const;

	unsigned scr_structure() const;
	unsigned sd_spec() const;
	unsigned data_stat_after_erase() const;
	unsigned sd_security() const;
	unsigned sd_bus_widths() const;
	unsigned sd_spec3() const;
	unsigned ex_security() const;
	unsigned sd_spec4() const;
	unsigned sd_specx() const;
	unsigned cmd_support() const;

private:
	alignas(4) std::array<std::byte, 8> r_;
};

/*
 * SD Card Status
 */
class card_status {
public:
	card_status();
	card_status(void *);

	bool out_of_range() const;
	bool address_error() const;
	bool block_len_error() const;
	bool erase_seq_error() const;
	bool erase_param() const;
	bool wp_violation() const;
	bool card_is_locked() const;
	bool lock_unlock_failed() const;
	bool com_crc_error() const;
	bool illegal_command() const;
	bool card_ecc_failed() const;
	bool cc_error() const;
	bool error() const;
	bool deferred_response() const;	/* eSD only */
	bool csd_overwrite() const;
	bool wp_erase_skip() const;
	bool card_ecc_disabled() const;	/* SD only */
	bool erase_reset() const;
	unsigned current_state() const;
	bool ready_for_data() const;
	bool fx_event() const;
	bool app_cmd() const;
	bool ake_seq_error() const;	/* SD only */

	bool any_error() const;

private:
	uint32_t r_;
};

/*
 * SD Card Function Status
 */
enum class access_mode {
	default_sdr12 = 0,
	high_sdr25 = 1,
	sdr50 = 2,
	sdr104 = 3,
	ddr50 = 4,
};
bool ddr_mode(access_mode);
const char *to_string(access_mode);

enum class driver_strength {
	type_b_50_ohm = 0,
	type_a_33_ohm = 1,
	type_c_66_ohm = 2,
	type_d_100_ohm = 3,
};
unsigned output_impedance(driver_strength);

enum class power_limit {
	w_0_72 = 0,
	w_1_44 = 1,
	w_2_16 = 2,
	w_2_88 = 3,
	w_1_80 = 4,
};

class function_status final {
public:
	function_status();

	void *data();
	size_t size() const;

	unsigned max_power() const;
	unsigned function_6_support() const;
	unsigned function_5_support() const;
	bitfield<sd::power_limit> power_limit() const;
	bitfield<sd::driver_strength> driver_strength() const;
	unsigned function_2_support() const;
	bitfield<sd::access_mode> access_mode() const;
	unsigned function_6_selection() const;
	unsigned function_5_selection() const;
	unsigned power_limit_selection() const;
	unsigned driver_strength_selection() const;
	unsigned command_system_selection() const;
	unsigned access_mode_selection() const;
	unsigned version() const;

private:
	alignas(4) std::array<std::byte, 64> r_;
};

/*
 * SD Status
 */
class status final {
public:
	status();

	void *data();
	size_t size() const;

	unsigned dat_bus_width() const;
	bool secured_mode() const;
	unsigned sd_card_type() const;
	unsigned long size_of_protected_area() const;
	unsigned speed_class() const;
	unsigned performance_move() const;
	unsigned au_size() const;
	unsigned erase_size() const;
	unsigned erase_timeout() const;
	unsigned erase_offset() const;
	unsigned uhs_speed_grade() const;
	unsigned uhs_au_size() const;
	unsigned video_speed_class() const;
	unsigned vsc_au_size() const;
	unsigned long sus_addr() const;
	unsigned app_perf_class() const;
	unsigned performance_enhance() const;
	bool discard_support() const;
	bool fule_support() const;

private:
	alignas(4) std::array<std::byte, 64> r_;
};

/*
 * SD Card Commands
 */
int go_idle_state(host *);
int send_if_cond(host *, float io_v);
int sd_send_op_cond(host *, bool s18r, float supply_v, ocr &);
int voltage_switch(host *);
int all_send_cid(host *, cid &);
int send_relative_addr(host *, unsigned &rca);
int select_deselect_card(host *, unsigned rca);
int send_status(host *, unsigned rca, card_status &);
int send_csd(host *, unsigned rca, csd &);
int send_scr(host *, unsigned rca, scr &);
int set_bus_width(host *, unsigned rca, unsigned width);
int check_func(host *, function_status &);
int switch_func(host *, power_limit, driver_strength, access_mode);
int sd_status(host *, unsigned rca, status &);
ssize_t read_single_block(host *h, const iovec *, size_t iov_off,
			  size_t len, size_t trfsz, size_t addr);
ssize_t read_multiple_block(host *, const iovec *, size_t iov_off,
			    size_t len, size_t trfsz, size_t addr);
ssize_t write_block(host *h, const iovec *, size_t iov_off, size_t len,
		    size_t trfsz, size_t addr);
ssize_t write_multiple_block(host *h, const iovec *, size_t iov_off,
			     size_t len, size_t trfsz, size_t addr);

constexpr unsigned tuning_cmd_index = 19;

}

#endif
