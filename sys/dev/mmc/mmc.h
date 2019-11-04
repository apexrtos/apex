#ifndef dev_mmc_mmc_h
#define dev_mmc_mmc_h

/*
 * MMC support
 */

#include "bitfield.h"
#include <array>
#include <string>

struct iovec;
namespace mmc { class host; }

namespace mmc::mmc {

/*
 * MMC Device Status
 */
enum class current_state {
	idle = 0,
	ready = 1,
	ident = 2,
	stby = 3,
	tran = 4,
	data = 5,
	rcv = 6,
	prg = 7,
	dis = 8,
	btst = 9,
	slp = 10,
	reserved_11 = 11,
	reserved_12 = 12,
	reserved_13 = 13,
	reserved_14 = 14,
	reserved_15 = 15,
};

class device_status {
public:
	device_status();
	device_status(void *);

	uint32_t raw() const;

	bool address_out_of_range() const;
	bool address_misalign() const;
	bool block_len_error() const;
	bool erase_seq_error() const;
	bool erase_param() const;
	bool wp_violation() const;
	bool device_is_locked() const;
	bool lock_unlock_failed() const;
	bool com_crc_error() const;
	bool illegal_command() const;
	bool device_ecc_failed() const;
	bool cc_error() const;
	bool error() const;
	bool cid_csd_overwrite() const;
	bool wp_erase_skip() const;
	bool erase_reset() const;
	mmc::current_state current_state() const;
	bool ready_for_data() const;
	bool switch_error() const;
	bool exception_event() const;
	bool app_cmd() const;

	bool any_error() const;

private:
	uint32_t r_;
};

/*
 * MMC Operating Conditions Register
 */
enum class access_mode {
	byte = 0,
	sector = 2,
};

class ocr final {
public:
	ocr();
	ocr(void *);

	bool v_170_195() const;
	bool v_200_210() const;
	bool v_210_220() const;
	bool v_220_230() const;
	bool v_230_240() const;
	bool v_240_250() const;
	bool v_250_260() const;
	bool v_260_270() const;
	bool v_270_280() const;
	bool v_280_290() const;
	bool v_290_300() const;
	bool v_300_310() const;
	bool v_310_320() const;
	bool v_320_330() const;
	bool v_330_340() const;
	bool v_340_350() const;
	bool v_350_360() const;
	mmc::access_mode access_mode() const;
	bool busy() const;

	bool supply_compatible(float supply_v) const;

private:
	uint32_t r_;
};

/*
 * MMC Device Identification Register
 */
class cid final {
public:
	cid();
	cid(void *);

	void clear();

	unsigned mid() const;
	unsigned bin() const;
	unsigned cbx() const;
	unsigned oid() const;
	std::string_view pnm() const;
	unsigned prv() const;
	unsigned long psn() const;
	unsigned mdt() const;

private:
	alignas(4) std::array<std::byte, 16> r_;
};

/*
 * MMC Device Specific Data Register
 */
class csd final {
public:
	csd();
	csd(void *);

	unsigned csd_structure() const;
	unsigned spec_vers() const;
	unsigned tacc() const;
	unsigned nsac() const;
	unsigned tran_speed() const;
	unsigned ccc() const;
	unsigned read_bl_len() const;
	bool read_bl_partial() const;
	bool write_blk_misalign() const;
	bool read_blk_misalign() const;
	bool dsr_imp() const;
	unsigned c_size() const;
	unsigned vdd_r_curr_min() const;
	unsigned vdd_r_curr_max() const;
	unsigned vdd_w_curr_min() const;
	unsigned vdd_w_curr_max() const;
	unsigned c_size_mult() const;
	unsigned erase_grp_size() const;
	unsigned erase_grp_mult() const;
	unsigned wp_grp_size() const;
	bool wp_grp_enable() const;
	unsigned default_ecc() const;
	unsigned r2w_factor() const;
	unsigned write_bl_len() const;
	bool write_bl_partial() const;
	bool content_prot_app() const;
	bool file_format_grp() const;
	bool copy() const;
	bool perm_write_protect() const;
	bool tmp_write_protect() const;
	unsigned file_format() const;
	unsigned ecc() const;

private:
	alignas(4) std::array<std::byte, 16> r_;
};

/*
 * MMC Extended Card Specific Data Register
 */
enum class device_type {
	sdr26 = 0,
	sdr52 = 1,
	ddr52_1v8_3v3 = 2,
	ddr52_1v2 = 3,
	hs200_1v8 = 4,
	hs200_1v2 = 5,
	hs400_1v8 = 6,
	hs400_1v2 = 7,
};
unsigned timing_interface(device_type);
bool ddr_mode(device_type);
bool hs_mode(device_type);
unsigned bus_mode(device_type, unsigned bus_width);
const char *to_string(device_type);

enum class driver_strength {
	type_0_50_ohm = 0,
	type_1_33_ohm = 1,
	type_2_66_ohm = 2,
	type_3_100_ohm = 3,
	type_4_40_ohm = 4,
};
unsigned output_impedance(driver_strength);

enum class rst_n_function {
	temporarily_disabled = 0,
	permanently_enabled = 1,
	permanently_disabled = 2,
	reserved = 3,
};
const char *to_string(rst_n_function);

enum class cache_ctrl {
	off = 0,
	on = 1,
};
const char *to_string(cache_ctrl);

enum class partition {
	user = 0,
	boot1 = 1,
	boot2 = 2,
	rpmb = 3,
	gp1 = 4,
	gp2 = 5,
	gp3 = 6,
	gp4 = 7,
};

enum class ext_partitions_attribute {
	none = 0,
	system_code = 1,
	non_persistent = 2,
	reserved = 3,
};

class ext_csd final {
public:
	/* Writable registers in the modes segment. */
	enum class offset {
		cmd_set = 191,
		power_class = 187,
		hs_timing = 185,
		bus_width = 183,
		partition_config = 179,
		boot_config_prot = 178,
		boot_bus_conditions = 177,
		erase_group_def = 175,
		boot_wp = 173,
		user_wp = 171,
		fw_config = 169,
		wr_rel_set = 167,
		sanitize_start = 165,
		bkops_start = 164,
		bkops_en = 163,
		rst_n_function = 162,
		hpi_mgmt = 161,
		partitions_attribute = 156,
		partition_setting_completed = 155,
		gp_size_mult_gpp1 = 143,
		gp_size_mult_gpp2 = 146,
		gp_size_mult_gpp3 = 149,
		gp_size_mult_gpp4 = 152,
		enh_size_mult = 140,
		sec_bad_blk_mgmt = 134,
		production_state_awareness = 133,
		tcase_support = 132,
		periodic_wakeup = 131,
		use_native_sector = 62,
		class_6_ctrl = 59,
		exception_events_ctrl = 56,
		ext_partitions_attribute = 52,
		context_conf = 37,
		power_off_notification = 34,
		cache_ctrl = 33,
		flush_cache = 32,
		barrier_ctrl = 31,
		mode_config = 30,
		mode_operation_codes = 29,
		pre_loading_data_size = 22,
		product_state_awareness_enablement = 17,
		secure_removal_type = 16,
		cmdq_mode_en = 15,
	};

	ext_csd();

	void *data();
	size_t size() const;

	/* Readable registers. */
	unsigned ext_security_err() const;
	unsigned s_cmd_set() const;
	unsigned hpi_features() const;
	unsigned bkops_support() const;
	unsigned max_packed_reads() const;
	unsigned max_packed_writes() const;
	unsigned data_tag_support() const;
	unsigned tag_unit_size() const;
	unsigned tag_res_size() const;
	unsigned context_capabilities() const;
	unsigned large_unit_size_m1() const;
	unsigned ext_support() const;
	unsigned supported_modes() const;
	unsigned ffu_features() const;
	unsigned operation_code_timeout() const;
	unsigned long ffu_arg() const;
	unsigned barrier_support() const;
	unsigned cmdq_support() const;
	unsigned cmdq_depth() const;
	unsigned long number_of_fw_sectors_correctly_programmed() const;
	std::array<std::byte, 32> vendor_proprietary_health_report() const;
	unsigned device_life_time_est_typ_b() const;
	unsigned device_life_time_est_typ_a() const;
	unsigned pre_eol_info() const;
	unsigned optimal_read_size() const;
	unsigned optimal_write_size() const;
	unsigned optimal_trim_unit_size() const;
	unsigned device_version() const;
	std::array<std::byte, 8> firmware_version() const;
	unsigned pwr_cl_ddr_200_360() const;
	unsigned long cache_size() const;
	unsigned generic_cmd6_time() const;
	unsigned power_off_long_time() const;
	unsigned bkops_status() const;
	unsigned long correctly_prg_sectors_num() const;
	unsigned ini_timeout_ap() const;
	unsigned cache_flush_policy() const;
	unsigned pwr_cl_ddr_52_360() const;
	unsigned pwr_cl_ddr_52_195() const;
	unsigned pwr_cl_200_195() const;
	unsigned pwr_cl_200_130() const;
	unsigned min_perf_ddr_w_8_52() const;
	unsigned min_perf_ddr_r_8_52() const;
	unsigned trim_mult() const;
	unsigned sec_feature_support() const;
	unsigned sec_erase_mult() const;
	unsigned sec_trim_mult() const;
	unsigned boot_info() const;
	unsigned boot_size_mult() const;
	unsigned acc_size() const;
	unsigned hc_erase_grp_size() const;
	unsigned erase_timeout_mult() const;
	unsigned rel_wr_sec_c() const;
	unsigned hc_wp_grp_size() const;
	unsigned s_c_vcc() const;
	unsigned s_c_vccq() const;
	unsigned production_state_awareness_timeout() const;
	unsigned s_a_timeout() const;
	unsigned sleep_notification_time() const;
	unsigned long sec_count() const;
	unsigned secure_wp_info() const;
	unsigned min_perf_w_8_52() const;
	unsigned min_perf_r_8_52() const;
	unsigned min_perf_w_8_26_4_52() const;
	unsigned min_perf_r_8_26_4_52() const;
	unsigned min_perf_w_4_26() const;
	unsigned min_perf_r_4_26() const;
	unsigned pwr_cl_26_360() const;
	unsigned pwr_cl_52_360() const;
	unsigned pwr_cl_26_195() const;
	unsigned pwr_cl_52_195() const;
	unsigned partition_switch_time() const;
	unsigned out_of_interrupt_time() const;
	bitfield<mmc::driver_strength> driver_strength() const;
	bitfield<mmc::device_type> device_type() const;
	unsigned csd_structure() const;
	unsigned ext_csd_rev() const;
	unsigned cmd_set() const;
	unsigned cmd_set_rev() const;
	unsigned power_class() const;
	unsigned hs_timing() const;
	unsigned strobe_support() const;
	unsigned bus_width() const;
	unsigned erased_mem_cont() const;
	unsigned partition_config() const;
	unsigned boot_config_prot() const;
	unsigned boot_bus_conditions() const;
	unsigned erase_group_def() const;
	unsigned boot_wp_status() const;
	unsigned boot_wp() const;
	unsigned user_wp() const;
	unsigned fw_config() const;
	unsigned rpmb_size_mult() const;
	unsigned wr_rel_set() const;
	unsigned wr_rel_param() const;
	unsigned bkops_en() const;
	mmc::rst_n_function rst_n_function() const;
	unsigned hpi_mgmt() const;
	unsigned partitioning_support() const;
	unsigned long max_enh_size_mult() const;
	unsigned partitions_attribute(partition) const;
	unsigned partitions_attribute() const;
	unsigned partition_setting_completed() const;
	unsigned long gp_size_mult_gpp(partition) const;
	unsigned long gp_size_mult_gpp1() const;
	unsigned long gp_size_mult_gpp2() const;
	unsigned long gp_size_mult_gpp3() const;
	unsigned long gp_size_mult_gpp4() const;
	unsigned long enh_size_mult() const;
	unsigned long enh_start_addr() const;
	unsigned sec_bad_blk_mgmnt() const;
	unsigned production_state_awareness() const;
	unsigned periodic_wakeup() const;
	unsigned program_cid_csd_ddr_support() const;
	std::array<std::byte, 64> vendor_specific_field() const;
	unsigned native_sector_size() const;
	unsigned use_native_sector() const;
	unsigned data_sector_size() const;
	unsigned ini_timeout_emu() const;
	unsigned class_6_ctrl() const;
	unsigned dyncap_needed() const;
	unsigned exception_events_ctrl() const;
	unsigned exception_events_status() const;
	mmc::ext_partitions_attribute ext_partitions_attribute(partition) const;
	unsigned ext_partitions_attribute() const;
	std::array<std::byte, 15> context_conf() const;
	unsigned packed_command_status() const;
	unsigned packed_failure_index() const;
	unsigned power_off_notification() const;
	mmc::cache_ctrl cache_ctrl() const;
	unsigned flush_cache() const;
	unsigned barrier_ctrl() const;
	unsigned mode_config() const;
	unsigned ffu_status() const;
	unsigned long pre_loading_data_size() const;
	unsigned long max_pre_loading_data_size() const;
	unsigned product_state_awareness_enablement() const;
	unsigned secure_removal_type() const;
	unsigned cmdq_mode_en() const;

	int write(host *, unsigned rca, offset, uint8_t);

private:
	alignas(4) std::array<std::byte, 512> r_;
};

/*
 * MMC Device Commands
 */
int go_idle_state(host *);
int app_cmd(host *, unsigned rca);
int send_op_cond(host *h, float supply_v, ocr &);
int all_send_cid(host *, cid &);
int set_relative_addr(host *h, unsigned rca);
int send_csd(host *h, unsigned rca, csd &);
int send_ext_csd(host *h, ext_csd &);
int select_deselect_card(host *, unsigned rca);
int send_status(host *, unsigned rca, device_status &);
int bus_test(host *, unsigned rca, unsigned bus_width);
ssize_t read_single_block(host *h, const iovec *, size_t iov_off, size_t len,
			  size_t trfsz, size_t addr);
ssize_t read_multiple_block(host *, const iovec *, size_t iov_off, size_t len,
			    size_t trfsz, size_t addr);
ssize_t write_block(host *h, const iovec *, size_t iov_off, size_t len,
		    size_t trfsz, size_t addr);
ssize_t write_multiple_block(host *h, const iovec *, size_t iov_off,
			     size_t len, size_t trfsz, size_t addr);

constexpr unsigned tuning_cmd_index = 21;

}

#endif
