#include "mmc.h"

#include "command.h"
#include "extract.h"
#include "host.h"
#include <cerrno>
#include <debug.h>

namespace mmc::mmc {

/*
 * device_status
 */
device_status::device_status()
{ }

device_status::device_status(void *p)
{
	memcpy(&r_, p, 4);
	r_ = be32toh(r_);

	if (any_error())
		dbg("device_status error %x\n", r_);
}

uint32_t
device_status::raw() const
{
	return r_;
}

bool device_status::address_out_of_range() const    { return bits(r_, 31); }
bool device_status::address_misalign() const	    { return bits(r_, 30); }
bool device_status::block_len_error() const	    { return bits(r_, 29); }
bool device_status::erase_seq_error() const	    { return bits(r_, 28); }
bool device_status::erase_param() const		    { return bits(r_, 27); }
bool device_status::wp_violation() const	    { return bits(r_, 26); }
bool device_status::device_is_locked() const	    { return bits(r_, 25); }
bool device_status::lock_unlock_failed() const	    { return bits(r_, 24); }
bool device_status::com_crc_error() const	    { return bits(r_, 23); }
bool device_status::illegal_command() const	    { return bits(r_, 22); }
bool device_status::device_ecc_failed() const	    { return bits(r_, 21); }
bool device_status::cc_error() const		    { return bits(r_, 20); }
bool device_status::error() const		    { return bits(r_, 19); }
bool device_status::cid_csd_overwrite() const	    { return bits(r_, 16); }
bool device_status::wp_erase_skip() const	    { return bits(r_, 15); }
bool device_status::erase_reset() const		    { return bits(r_, 13); }

current_state
device_status::current_state() const
{
	return static_cast<mmc::current_state>(bits(r_, 9, 12));
}

bool device_status::ready_for_data() const	    { return bits(r_, 8); }
bool device_status::switch_error() const	    { return bits(r_, 7); }
bool device_status::exception_event() const	    { return bits(r_, 6); }
bool device_status::app_cmd() const		    { return bits(r_, 5); }

bool
device_status::any_error() const
{
	/* See comments in sd.cpp for why we do this. */
	return r_ & 0b11111101111110011010000010000000;
}

/*
 * ocr
 */
ocr::ocr()
{ }

ocr::ocr(void *p)
{
	memcpy(&r_, p, 4);
	r_ = be32toh(r_);
}

bool ocr::v_170_195() const		{ return bits(r_, 7); }
bool ocr::v_200_210() const		{ return bits(r_, 8); }
bool ocr::v_210_220() const		{ return bits(r_, 9); }
bool ocr::v_220_230() const		{ return bits(r_, 10); }
bool ocr::v_230_240() const		{ return bits(r_, 11); }
bool ocr::v_240_250() const		{ return bits(r_, 12); }
bool ocr::v_250_260() const		{ return bits(r_, 13); }
bool ocr::v_260_270() const		{ return bits(r_, 14); }
bool ocr::v_270_280() const		{ return bits(r_, 15); }
bool ocr::v_280_290() const		{ return bits(r_, 16); }
bool ocr::v_290_300() const		{ return bits(r_, 17); }
bool ocr::v_300_310() const		{ return bits(r_, 18); }
bool ocr::v_310_320() const		{ return bits(r_, 19); }
bool ocr::v_320_330() const		{ return bits(r_, 20); }
bool ocr::v_330_340() const		{ return bits(r_, 21); }
bool ocr::v_340_350() const		{ return bits(r_, 22); }
bool ocr::v_350_360() const		{ return bits(r_, 23); }
bool ocr::busy() const			{ return !bits(r_, 31); }

access_mode
ocr::access_mode() const
{
	return static_cast<mmc::access_mode>(bits(r_, 29, 30));
}

bool
ocr::supply_compatible(float supply_v) const
{
	if (supply_v < 1.7 || supply_v > 3.6)
		return false;
	if (supply_v <= 1.95)
		return v_170_195();
	return r_ & (0x80 << ((int)(supply_v * 10) - 20));
}

/*
 * cid
 */
cid::cid()
{ }

cid::cid(void *p)
{
	memcpy(r_.data(), p, r_.size());
}

void
cid::clear()
{
	memset(r_.data(), 0, r_.size());
}

unsigned cid::mid() const	    { return bits(r_, 120, 127); }
unsigned cid::bin() const	    { return bits(r_, 114, 119); }
unsigned cid::cbx() const	    { return bits(r_, 112, 113); }
unsigned cid::oid() const	    { return bits(r_, 104, 111); }
std::string_view cid::pnm() const   { return {(const char *)(r_.data()) + 3, 6}; }
unsigned cid::prv() const	    { return bits(r_, 48, 55); }
unsigned long cid::psn() const	    { return bits(r_, 16, 47); }
unsigned cid::mdt() const	    { return bits(r_, 8, 15); }

/*
 * csd
 */
csd::csd()
{ }

csd::csd(void *p)
{
	memcpy(r_.data(), p, r_.size());
}

unsigned csd::csd_structure() const	{ return bits(r_, 126, 127); }
unsigned csd::spec_vers() const		{ return bits(r_, 122, 125); }
unsigned csd::tacc() const		{ return bits(r_, 112, 119); }
unsigned csd::nsac() const		{ return bits(r_, 104, 111); }
unsigned csd::tran_speed() const	{ return bits(r_, 96, 103); }
unsigned csd::ccc() const		{ return bits(r_, 84, 95); }
unsigned csd::read_bl_len() const	{ return bits(r_, 80, 83); }
bool csd::read_bl_partial() const	{ return bits(r_, 79); }
bool csd::write_blk_misalign() const	{ return bits(r_, 78); }
bool csd::read_blk_misalign() const	{ return bits(r_, 77); }
bool csd::dsr_imp() const		{ return bits(r_, 76); }
unsigned csd::c_size() const		{ return bits(r_, 62, 73); }
unsigned csd::vdd_r_curr_min() const	{ return bits(r_, 59, 61); }
unsigned csd::vdd_r_curr_max() const	{ return bits(r_, 56, 58); }
unsigned csd::vdd_w_curr_min() const	{ return bits(r_, 53, 55); }
unsigned csd::vdd_w_curr_max() const	{ return bits(r_, 50, 52); }
unsigned csd::c_size_mult() const	{ return bits(r_, 47, 49); }
unsigned csd::erase_grp_size() const	{ return bits(r_, 42, 46); }
unsigned csd::erase_grp_mult() const	{ return bits(r_, 37, 41); }
unsigned csd::wp_grp_size() const	{ return bits(r_, 32, 36); }
bool csd::wp_grp_enable() const		{ return bits(r_, 31); }
unsigned csd::default_ecc() const	{ return bits(r_, 29, 30); }
unsigned csd::r2w_factor() const	{ return bits(r_, 26, 28); }
unsigned csd::write_bl_len() const	{ return bits(r_, 22, 25); }
bool csd::write_bl_partial() const	{ return bits(r_, 21); }
bool csd::content_prot_app() const	{ return bits(r_, 16); }
bool csd::file_format_grp() const	{ return bits(r_, 15); }
bool csd::copy() const			{ return bits(r_, 14); }
bool csd::perm_write_protect() const	{ return bits(r_, 13); }
bool csd::tmp_write_protect() const	{ return bits(r_, 12); }
unsigned csd::file_format() const	{ return bits(r_, 10, 11); }
unsigned csd::ecc() const		{ return bits(r_, 8, 9); }

/*
 * timing_interface
 */
unsigned
timing_interface(device_type d)
{
	switch (d) {
	case device_type::sdr26:
		return 0;
	case device_type::sdr52:
	case device_type::ddr52_1v8_3v3:
	case device_type::ddr52_1v2:
		return 1;
	case device_type::hs200_1v8:
	case device_type::hs200_1v2:
		return 2;
	case device_type::hs400_1v8:
	case device_type::hs400_1v2:
		return 3;
	default:
		__builtin_unreachable();
	}
}

/*
 * ddr_mode
 */
bool
ddr_mode(device_type d)
{
	switch (d) {
	case device_type::sdr26:
	case device_type::sdr52:
	case device_type::hs200_1v8:
	case device_type::hs200_1v2:
		return false;
	case device_type::ddr52_1v8_3v3:
	case device_type::ddr52_1v2:
	case device_type::hs400_1v8:
	case device_type::hs400_1v2:
		return true;
	default:
		__builtin_unreachable();
	}
}

/*
 * hs_mode
 */
bool
hs_mode(device_type d)
{
	switch (d) {
	case device_type::sdr26:
	case device_type::sdr52:
	case device_type::ddr52_1v8_3v3:
	case device_type::ddr52_1v2:
		return false;
	case device_type::hs200_1v8:
	case device_type::hs200_1v2:
	case device_type::hs400_1v8:
	case device_type::hs400_1v2:
		return true;
	default:
		__builtin_unreachable();
	}
}

/*
 * bus_mode
 */
unsigned
bus_mode(device_type d, unsigned bus_width)
{
	unsigned r = bus_width / 4;

	if (ddr_mode(d)) {
		/* DDR only valid for 4- and 8-bit bus. */
		assert(bus_width >= 4);
		r |= 4;
	}

	return r;
}

/*
 * to_string
 */
const char *
to_string(device_type d)
{
	switch (d) {
	case device_type::sdr26:
		return "SDR26";
	case device_type::sdr52:
		return "SDR52";
	case device_type::ddr52_1v8_3v3:
	case device_type::ddr52_1v2:
		return "DDR52";
	case device_type::hs200_1v8:
	case device_type::hs200_1v2:
		return "HS200";
	case device_type::hs400_1v8:
	case device_type::hs400_1v2:
		return "HS400";
	default:
		__builtin_unreachable();
	}
}

/*
 * output_impedance
 */
unsigned
output_impedance(driver_strength v)
{
	switch (v) {
	case driver_strength::type_0_50_ohm: return 50;
	case driver_strength::type_1_33_ohm: return 33;
	case driver_strength::type_2_66_ohm: return 66;
	case driver_strength::type_3_100_ohm: return 100;
	case driver_strength::type_4_40_ohm: return 40;
	default: __builtin_unreachable();
	}
}

/*
 * to_string
 */
const char *
to_string(rst_n_function v)
{
	switch (v) {
	case rst_n_function::temporarily_disabled:
		return "temporarily disabled";
	case rst_n_function::permanently_enabled:
		return "permanently enabled";
	case rst_n_function::permanently_disabled:
		return "permanently disabled";
	case rst_n_function::reserved:
		return "reserved";
	default:
		__builtin_unreachable();
	}
}

/*
 * to_string
 */
const char *
to_string(cache_ctrl v)
{
	switch (v) {
	case cache_ctrl::off:
		return "off";
	case cache_ctrl::on:
		return "on";
	default:
		__builtin_unreachable();
	}
}

/*
 * ext_csd
 */
ext_csd::ext_csd()
{ }

void *
ext_csd::data()
{
	return r_.data();
}

size_t
ext_csd::size() const
{
	return r_.size();
}

unsigned ext_csd::ext_security_err() const	    { return bytes(r_, 505); }
unsigned ext_csd::s_cmd_set() const		    { return bytes(r_, 504); }
unsigned ext_csd::hpi_features() const		    { return bytes(r_, 503); }
unsigned ext_csd::bkops_support() const		    { return bytes(r_, 502); }
unsigned ext_csd::max_packed_reads() const	    { return bytes(r_, 501); }
unsigned ext_csd::max_packed_writes() const	    { return bytes(r_, 500); }
unsigned ext_csd::data_tag_support() const	    { return bytes(r_, 499); }
unsigned ext_csd::tag_unit_size() const		    { return bytes(r_, 498); }
unsigned ext_csd::tag_res_size() const		    { return bytes(r_, 497); }
unsigned ext_csd::context_capabilities() const	    { return bytes(r_, 496); }
unsigned ext_csd::large_unit_size_m1() const	    { return bytes(r_, 495); }
unsigned ext_csd::ext_support() const		    { return bytes(r_, 494); }
unsigned ext_csd::supported_modes() const	    { return bytes(r_, 493); }
unsigned ext_csd::ffu_features() const		    { return bytes(r_, 492); }
unsigned ext_csd::operation_code_timeout() const    { return bytes(r_, 491); }
unsigned long ext_csd::ffu_arg() const		    { return bytes(r_, 487, 490); }
unsigned ext_csd::barrier_support() const	    { return bytes(r_, 486); }
unsigned ext_csd::cmdq_support() const		    { return bytes(r_, 308); }
unsigned ext_csd::cmdq_depth() const		    { return bytes(r_, 307); }

unsigned long
ext_csd::number_of_fw_sectors_correctly_programmed() const
{
	return bytes(r_, 302, 305);
}

std::array<std::byte, 32>
ext_csd::vendor_proprietary_health_report() const
{
	std::array<std::byte, 32> r;
	memcpy(r.data(), r_.data() + 270, 32);
	return r;
}

unsigned
ext_csd::device_life_time_est_typ_b() const
{
	return bytes(r_, 269);
}

unsigned
ext_csd::device_life_time_est_typ_a() const
{
	return bytes(r_, 268);
}

unsigned ext_csd::pre_eol_info() const		    { return bytes(r_, 267); }
unsigned ext_csd::optimal_read_size() const	    { return bytes(r_, 266); }
unsigned ext_csd::optimal_write_size() const	    { return bytes(r_, 265); }
unsigned ext_csd::optimal_trim_unit_size() const    { return bytes(r_, 264); }
unsigned ext_csd::device_version() const	    { return bytes(r_, 262, 263); }

std::array<std::byte, 8>
ext_csd::firmware_version() const
{
	std::array<std::byte, 8> r;
	memcpy(r.data(), r_.data() + 254, 8);
	return r;
}

unsigned ext_csd::pwr_cl_ddr_200_360() const	    { return bytes(r_, 253); }
unsigned long ext_csd::cache_size() const	    { return bytes(r_, 249, 252); }
unsigned ext_csd::generic_cmd6_time() const	    { return bytes(r_, 248); }
unsigned ext_csd::power_off_long_time() const	    { return bytes(r_, 247); }
unsigned ext_csd::bkops_status() const		    { return bytes(r_, 246); }

unsigned long
ext_csd::correctly_prg_sectors_num() const
{
	return bytes(r_, 242, 245);
}

unsigned ext_csd::ini_timeout_ap() const	    { return bytes(r_, 241); }
unsigned ext_csd::cache_flush_policy() const	    { return bytes(r_, 240); }
unsigned ext_csd::pwr_cl_ddr_52_360() const	    { return bytes(r_, 239); }
unsigned ext_csd::pwr_cl_ddr_52_195() const	    { return bytes(r_, 238); }
unsigned ext_csd::pwr_cl_200_195() const	    { return bytes(r_, 237); }
unsigned ext_csd::pwr_cl_200_130() const	    { return bytes(r_, 236); }
unsigned ext_csd::min_perf_ddr_w_8_52() const	    { return bytes(r_, 235); }
unsigned ext_csd::min_perf_ddr_r_8_52() const	    { return bytes(r_, 234); }
unsigned ext_csd::trim_mult() const		    { return bytes(r_, 232); }

bitfield<sec_feature_support>
ext_csd::sec_feature_support() const
{
	return bytes(r_, 231);
}

unsigned ext_csd::sec_erase_mult() const	    { return bytes(r_, 230); }
unsigned ext_csd::sec_trim_mult() const		    { return bytes(r_, 229); }
unsigned ext_csd::boot_info() const		    { return bytes(r_, 228); }
unsigned ext_csd::boot_size_mult() const	    { return bytes(r_, 226); }
unsigned ext_csd::acc_size() const		    { return bytes(r_, 225); }
unsigned ext_csd::hc_erase_grp_size() const	    { return bytes(r_, 224); }
unsigned ext_csd::erase_timeout_mult() const	    { return bytes(r_, 223); }
unsigned ext_csd::rel_wr_sec_c() const		    { return bytes(r_, 222); }
unsigned ext_csd::hc_wp_grp_size() const	    { return bytes(r_, 221); }
unsigned ext_csd::s_c_vcc() const		    { return bytes(r_, 220); }
unsigned ext_csd::s_c_vccq() const		    { return bytes(r_, 219); }

unsigned
ext_csd::production_state_awareness_timeout() const
{
	return bytes(r_, 218);
}

unsigned ext_csd::s_a_timeout() const		    { return bytes(r_, 217); }
unsigned ext_csd::sleep_notification_time() const   { return bytes(r_, 216); }
unsigned long ext_csd::sec_count() const	    { return bytes(r_, 212, 215); }
unsigned ext_csd::secure_wp_info() const	    { return bytes(r_, 211); }
unsigned ext_csd::min_perf_w_8_52() const	    { return bytes(r_, 210); }
unsigned ext_csd::min_perf_r_8_52() const	    { return bytes(r_, 209); }
unsigned ext_csd::min_perf_w_8_26_4_52() const	    { return bytes(r_, 208); }
unsigned ext_csd::min_perf_r_8_26_4_52() const	    { return bytes(r_, 207); }
unsigned ext_csd::min_perf_w_4_26() const	    { return bytes(r_, 206); }
unsigned ext_csd::min_perf_r_4_26() const	    { return bytes(r_, 205); }
unsigned ext_csd::pwr_cl_26_360() const		    { return bytes(r_, 203); }
unsigned ext_csd::pwr_cl_52_360() const		    { return bytes(r_, 202); }
unsigned ext_csd::pwr_cl_26_195() const		    { return bytes(r_, 201); }
unsigned ext_csd::pwr_cl_52_195() const		    { return bytes(r_, 200); }
unsigned ext_csd::partition_switch_time() const     { return bytes(r_, 199); }
unsigned ext_csd::out_of_interrupt_time() const     { return bytes(r_, 198); }

bitfield<driver_strength>
ext_csd::driver_strength() const
{
	return bytes(r_, 197);
}

bitfield<device_type> ext_csd::device_type() const  { return bytes(r_, 196); }
unsigned ext_csd::csd_structure() const		    { return bytes(r_, 194); }
unsigned ext_csd::ext_csd_rev() const		    { return bytes(r_, 192); }
unsigned ext_csd::cmd_set() const		    { return bytes(r_, 191); }
unsigned ext_csd::cmd_set_rev() const		    { return bytes(r_, 189); }
unsigned ext_csd::power_class() const		    { return bytes(r_, 187); }
unsigned ext_csd::hs_timing() const		    { return bytes(r_, 185); }
unsigned ext_csd::strobe_support() const	    { return bytes(r_, 184); }
unsigned ext_csd::bus_width() const		    { return bytes(r_, 183); }
unsigned ext_csd::erased_mem_cont() const	    { return bytes(r_, 181); }
unsigned ext_csd::partition_config() const	    { return bytes(r_, 179); }
unsigned ext_csd::boot_config_prot() const	    { return bytes(r_, 178); }
unsigned ext_csd::boot_bus_conditions() const	    { return bytes(r_, 177); }
unsigned ext_csd::erase_group_def() const	    { return bytes(r_, 175); }
unsigned ext_csd::boot_wp_status() const	    { return bytes(r_, 174); }
unsigned ext_csd::boot_wp() const		    { return bytes(r_, 173); }
unsigned ext_csd::user_wp() const		    { return bytes(r_, 171); }
unsigned ext_csd::fw_config() const		    { return bytes(r_, 169); }
unsigned ext_csd::rpmb_size_mult() const	    { return bytes(r_, 168); }
unsigned ext_csd::wr_rel_set() const		    { return bytes(r_, 167); }
unsigned ext_csd::wr_rel_param() const		    { return bytes(r_, 166); }
unsigned ext_csd::bkops_en() const		    { return bytes(r_, 163); }

rst_n_function
ext_csd::rst_n_function() const
{
	return static_cast<mmc::rst_n_function>(bytes(r_, 162) & 3);
}

unsigned ext_csd::hpi_mgmt() const		    { return bytes(r_, 161); }
unsigned ext_csd::partitioning_support() const	    { return bytes(r_, 160); }
unsigned long ext_csd::max_enh_size_mult() const    { return bytes(r_, 157, 159); }

unsigned
ext_csd::partitions_attribute(partition p) const
{
	auto attr = partitions_attribute();

	switch (p) {
	case partition::user:
		return attr & 1;
	case partition::gp1:
	case partition::gp2:
	case partition::gp3:
	case partition::gp4:
		return (attr >> (static_cast<int>(p) - 3)) & 1;
	default:
		assert(0);
	};
}

unsigned ext_csd::partitions_attribute() const	    { return bytes(r_, 156); }

unsigned
ext_csd::partition_setting_completed() const
{
	return bytes(r_, 155);
}

unsigned long
ext_csd::gp_size_mult_gpp(partition p) const
{
	auto n = static_cast<int>(p);
	assert(n >= 4);
	n = (n - 4) * 3;
	return bytes(r_, 143 + n, 145 + n);
}

unsigned long ext_csd::gp_size_mult_gpp1() const    { return bytes(r_, 143, 145); }
unsigned long ext_csd::gp_size_mult_gpp2() const    { return bytes(r_, 146, 148); }
unsigned long ext_csd::gp_size_mult_gpp3() const    { return bytes(r_, 149, 151); }
unsigned long ext_csd::gp_size_mult_gpp4() const    { return bytes(r_, 152, 154); }
unsigned long ext_csd::enh_size_mult() const	    { return bytes(r_, 140, 142); }
unsigned long ext_csd::enh_start_addr() const	    { return bytes(r_, 136, 139); }
unsigned ext_csd::sec_bad_blk_mgmnt() const	    { return bytes(r_, 134); }

unsigned
ext_csd::production_state_awareness() const
{
	return bytes(r_, 133);
}

unsigned ext_csd::periodic_wakeup() const	    { return bytes(r_, 131); }

unsigned
ext_csd::program_cid_csd_ddr_support() const
{
	return bytes(r_, 130);
}

std::array<std::byte, 64>
ext_csd::vendor_specific_field() const
{
	std::array<std::byte, 64> r;
	memcpy(r.data(), r_.data() + 64, 64);
	return r;
}

unsigned ext_csd::native_sector_size() const	    { return bytes(r_, 63); }
unsigned ext_csd::use_native_sector() const	    { return bytes(r_, 62); }
unsigned ext_csd::data_sector_size() const	    { return bytes(r_, 61); }
unsigned ext_csd::ini_timeout_emu() const	    { return bytes(r_, 60); }
unsigned ext_csd::class_6_ctrl() const		    { return bytes(r_, 59); }
unsigned ext_csd::dyncap_needed() const		    { return bytes(r_, 58); }
unsigned ext_csd::exception_events_ctrl() const	    { return bytes(r_, 56, 57); }
unsigned ext_csd::exception_events_status() const   { return bytes(r_, 54, 55); }

ext_partitions_attribute
ext_csd::ext_partitions_attribute(partition p) const
{
	auto attr = ext_partitions_attribute();

	switch (p) {
	case partition::gp1:
	case partition::gp2:
	case partition::gp3:
	case partition::gp4:
		attr = (attr >> ((static_cast<int>(p) - 4) * 4)) & 0xf;
		if (attr >= 3)
			return mmc::ext_partitions_attribute::reserved;
		return static_cast<mmc::ext_partitions_attribute>(attr);
	default:
		assert(0);
	}
}

unsigned ext_csd::ext_partitions_attribute() const  { return bytes(r_, 52, 53); }

std::array<std::byte, 15>
ext_csd::context_conf() const
{
	std::array<std::byte, 15> r;
	memcpy(r.data(), r_.data() + 37, 15);
	return r;
}

unsigned ext_csd::packed_command_status() const	    { return bytes(r_, 36); }
unsigned ext_csd::packed_failure_index() const	    { return bytes(r_, 35); }
unsigned ext_csd::power_off_notification() const    { return bytes(r_, 34); }

cache_ctrl
ext_csd::cache_ctrl() const
{
	return static_cast<mmc::cache_ctrl>(bytes(r_, 33) & 1);
}

unsigned ext_csd::flush_cache() const		    { return bytes(r_, 32); }
unsigned ext_csd::barrier_ctrl() const		    { return bytes(r_, 31); }
unsigned ext_csd::mode_config() const		    { return bytes(r_, 30); }
unsigned ext_csd::ffu_status() const		    { return bytes(r_, 26); }

unsigned long
ext_csd::pre_loading_data_size() const
{
	return bytes(r_, 22, 25);
}

unsigned long
ext_csd::max_pre_loading_data_size() const
{
	return bytes(r_, 18, 21);
}

unsigned
ext_csd::product_state_awareness_enablement() const
{
	return bytes(r_, 17);
}

unsigned ext_csd::secure_removal_type() const	    { return bytes(r_, 16); }
unsigned ext_csd::cmdq_mode_en() const		    { return bytes(r_, 15); }

/*
 * ext_csd::write
 */
int
ext_csd::write(host *h, unsigned rca, ext_csd::offset off, uint8_t value)
{
	const uint32_t access = 3; /* write byte */
	const uint32_t cmd_set = 0; /* normal */
	const uint32_t index = static_cast<uint32_t>(off);

	if (index > 255)
		return DERR(-EINVAL);

	command cmd{6,
	    access << 24 | index << 16 | value << 8 | cmd_set,
	    command::response_type::r1b};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	device_status s{cmd.response().data()};

	if (s.any_error())
		return DERR(-EIO);

	/* Check status again after busy signal clears. */
	if (auto r = send_status(h, rca, s); r < 0)
		return r;

	if (s.any_error())
		return DERR(-EIO);

	/* Update cache. */
	r_[index] = static_cast<std::byte>(value);

	return 0;
}


/*
 * go_idle_state
 */
int
go_idle_state(host *h)
{
	command cmd{0, 0, command::response_type::none};
	return h->run_command(cmd, 0);
}

/*
 * stop_transmission
 */
int
stop_transmission(host *h)
{
	command cmd{12, 0, command::response_type::r1b};
	return h->run_command(cmd, 0);
}

/*
 * app_cmd
 */
int
app_cmd(host *h, unsigned rca)
{
	command cmd{55, rca << 16, command::response_type::r1};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	if (!device_status{cmd.response().data()}.app_cmd())
		return DERR(-EIO);

	return 0;
}

/*
 * send_op_cond
 */
int
send_op_cond(host *h, float supply_v, ocr &o)
{
	if (supply_v != 0 && (supply_v < 1.7 || supply_v > 3.6))
		return DERR(-EINVAL);

	const uint32_t access_mode = static_cast<int>(access_mode::sector);
	const uint32_t voltage_window = supply_v == 0 ? 0 :
		supply_v < 2.0 ? 1 : 1ul << ((int)(supply_v * 10) - 20);
	command cmd{1,
	    access_mode << 29 | voltage_window << 7,
	    command::response_type::r3};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	o = ocr{cmd.response().data()};

	return 0;
}

/*
 * all_send_cid
 */
int
all_send_cid(host *h, cid &c)
{
	command cmd{2, 0, command::response_type::r2};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	c = cid{cmd.response().data()};

	return 0;
}

/*
 * set_relative_addr
 */
int
set_relative_addr(host *h, unsigned rca)
{
	command cmd{3, rca << 16, command::response_type::r1};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	if (device_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	return 0;
}

/*
 * send_csd
 */
int
send_csd(host *h, unsigned rca, csd &c)
{
	command cmd{9, rca << 16, command::response_type::r2};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	c = csd{cmd.response().data()};

	return 0;
}

/*
 * send_ext_csd
 */
int
send_ext_csd(host *h, ext_csd &c)
{
	iovec iov{c.data(), c.size()};
	command cmd{8, 0, command::response_type::r1};
	cmd.setup_data_transfer(command::data_direction::device_to_host,
	    c.size(), &iov, 0, c.size());

	if (auto r = h->run_command(cmd, 0); (size_t)r != c.size())
		return r < 0 ? r : DERR(-EIO);

	if (device_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	return 0;
}

/*
 * select_deselect_card
 */
int
select_deselect_card(host *h, unsigned rca)
{
	command cmd{7, rca << 16, command::response_type::r1b};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	device_status s{cmd.response().data()};

	if (s.any_error())
		return DERR(-EIO);
	if (s.device_is_locked())
		return DERR(-EACCES);

	return 0;
}

/*
 * send_status
 */
int
send_status(host *h, unsigned rca, device_status &s)
{
	const uint32_t sqs = 0;	/* status query */
	const uint32_t hpi = 0; /* no interrupt */

	/* Technically send_status does not use busy signalling, but by
	 * indicating that it does we guarantee any previous command is
	 * completed before reading the status register. */
	command cmd{13,
	    rca << 16 | sqs << 15 | hpi,
	    command::response_type::r1b};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	s = device_status{cmd.response().data()};

	return 0;
}

/*
 * bus_test
 */
int
bus_test(host *h, unsigned rca, const unsigned w)
{
	iovec iov;
	char buf[16]{};
	device_status s;

	char *tx = buf;
	char *rx = tx + 8;

	switch (w) {
	case 4:
		memcpy(tx, (const char[]){0x5a}, 1);
		break;
	case 8:
		memcpy(tx, (const char[]){0x55, 0xaa}, 2);
		break;
	default:
		return DERR(-EINVAL);
	}

	/* Write bus test data to device & verify bus test state. */
	iov = {tx, w};
	command write{19, 0, command::response_type::r1};
	write.setup_data_transfer(command::data_direction::host_to_device,
	    w, &iov, 0, w);
	if (auto r = h->run_command(write, 0); (size_t)r != w)
		return r < 0 ? r : DERR(-EIO);
	if (auto r = send_status(h, rca, s); r < 0)
		return r;
	if (s.current_state() != current_state::btst)
		return DERR(-EIO);

	/* Read bus test data from device & verify transfer state. */
	iov = {rx, w};
	command read{14, 0, command::response_type::r1};
	read.setup_data_transfer(command::data_direction::device_to_host,
	    w, &iov, 0, w);
	if (auto r = h->run_command(read, 0); (size_t)r != w)
		return r < 0 ? r : DERR(-EIO);
	if (auto r = send_status(h, rca, s); r < 0)
		return r;
	if (s.current_state() != current_state::tran)
		return DERR(-EIO);

	/* Compare bus test data. */
	for (size_t i = 0; i < w / 4; ++i)
		if ((tx[i] ^ rx[i]) != 0xff)
			return -EIO;

	return 0;
}

/*
 * do_transfer - helper
 */
static ssize_t
do_transfer(host *h, unsigned cmd_index, enum command::data_direction dir,
    const iovec *iov, size_t iov_off, size_t len, size_t trfsz,
    size_t addr)
{
	command cmd{cmd_index, addr, command::response_type::r1};
	cmd.setup_data_transfer(dir, trfsz, iov, iov_off, len);

	if (cmd.data_size() % trfsz)
		return DERR(-EINVAL);

	ssize_t r;

	if (r = h->run_command(cmd, 0); r < 0)
		return r;

	if (device_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	return r;
}

/*
 * read_single_block
 */
ssize_t
read_single_block(host *h, const iovec *iov, size_t iov_off, size_t len,
    size_t trfsz, size_t addr)
{
	return do_transfer(h, 17, command::data_direction::device_to_host,
	    iov, iov_off, len, trfsz, addr);
}

/*
 * read_multiple_block
 */
ssize_t
read_multiple_block(host *h, const iovec *iov, size_t iov_off, size_t len,
    size_t trfsz, size_t addr)
{
	return do_transfer(h, 18, command::data_direction::device_to_host,
	    iov, iov_off, len, trfsz, addr);
}

/*
 * write_block
 */
ssize_t
write_block(host *h, const iovec *iov, size_t iov_off, size_t len,
    size_t trfsz, size_t addr)
{
	return do_transfer(h, 24, command::data_direction::host_to_device,
	    iov, iov_off, len, trfsz, addr);
}

/*
 * write_multiple_block
 */
ssize_t
write_multiple_block(host *h, const iovec *iov, size_t iov_off, size_t len,
    size_t trfsz, size_t addr)
{
	return do_transfer(h, 25, command::data_direction::host_to_device,
	    iov, iov_off, len, trfsz, addr);
}

/*
 * erase_sequence - helper
 */
static int
erase_sequence(host *h, size_t start_lba, size_t end_lba, unsigned arg)
{
	/* CMD35 & CMD36 don't use busy signalling, however they cannot be
	 * issued while the device is in prg state so we use r1b response type
	 * to wait for tran state before issuing the command */

	/* set start lba */
	command erase_group_start{35, start_lba, command::response_type::r1b};
	if (auto r = h->run_command(erase_group_start, 0); r < 0)
		return r;
	if (device_status{erase_group_start.response().data()}.any_error())
		return DERR(-EIO);

	/* set end lba */
	command erase_group_end{36, end_lba, command::response_type::r1b};
	if (auto r = h->run_command(erase_group_end, 0); r < 0)
		return r;
	if (device_status{erase_group_end.response().data()}.any_error())
		return DERR(-EIO);

	/* issue erase */
	command erase{38, arg, command::response_type::r1b};
	if (auto r = h->run_command(erase, 0); r < 0)
		return r;
	if (device_status{erase.response().data()}.any_error())
		return DERR(-EIO);

	return 0;
}

/*
 * discard
 */
int
discard(host *h, size_t start_lba, size_t end_lba)
{
	return erase_sequence(h, start_lba, end_lba, 3);
}

/*
 * trim
 */
int
trim(host *h, size_t start_lba, size_t end_lba)
{
	return erase_sequence(h, start_lba, end_lba, 1);
}

}
