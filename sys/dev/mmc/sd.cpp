#include "sd.h"

#include "command.h"
#include "extract.h"
#include "host.h"
#include <cerrno>
#include <debug.h>

namespace mmc::sd {

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

bool ocr::v_270_280() const	    { return bits(r_, 15); }
bool ocr::v_280_290() const	    { return bits(r_, 16); }
bool ocr::v_290_300() const	    { return bits(r_, 17); }
bool ocr::v_300_310() const	    { return bits(r_, 18); }
bool ocr::v_310_320() const	    { return bits(r_, 19); }
bool ocr::v_320_330() const	    { return bits(r_, 20); }
bool ocr::v_330_340() const	    { return bits(r_, 21); }
bool ocr::v_340_350() const	    { return bits(r_, 22); }
bool ocr::v_350_360() const	    { return bits(r_, 23); }
bool ocr::s18a() const		    { return bits(r_, 24); }
bool ocr::uhs_ii_status() const     { return bits(r_, 29); }
bool ocr::ccs() const		    { return bits(r_, 30); }
bool ocr::busy() const		    { return !bits(r_, 31); }

bool
ocr::supply_compatible(float supply_v) const
{
	if (supply_v < 2.7 || supply_v > 3.6)
		return false;

	return r_ & (0x8000ul << ((int)(supply_v * 10) - 27));
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
unsigned cid::oid() const	    { return bits(r_, 104, 119); }
std::string_view cid::pnm() const   { return {(char *)(r_.data() + 3), 5}; }
unsigned cid::prv() const	    { return bits(r_, 56, 63); }
unsigned long cid::psn() const	    { return bits(r_, 24, 55); }
unsigned cid::mdt() const	    { return bits(r_, 8, 19); }

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
unsigned csd::ccc() const		{ return bits(r_, 84, 95); }
bool csd::dsr_imp() const		{ return bits(r_, 76); }
unsigned long csd::c_size() const	{ return bits(r_, 48, 69); }
bool csd::copy() const			{ return bits(r_, 14); }
bool csd::perm_write_protect() const	{ return bits(r_, 13); }
bool csd::tmp_write_protect() const	{ return bits(r_, 12); }

/*
 * scr
 */
scr::scr()
{ }

void *
scr::data()
{
	return r_.data();
}

size_t
scr::size() const
{
	return r_.size();
}

unsigned scr::scr_structure() const		{ return bits(r_, 60, 63); }
unsigned scr::sd_spec() const			{ return bits(r_, 56, 59); }
unsigned scr::data_stat_after_erase() const	{ return bits(r_, 55); }
unsigned scr::sd_security() const		{ return bits(r_, 52, 54); }
unsigned scr::sd_bus_widths() const		{ return bits(r_, 48, 51); }
unsigned scr::sd_spec3() const			{ return bits(r_, 47); }
unsigned scr::ex_security() const		{ return bits(r_, 43, 46); }
unsigned scr::sd_spec4() const			{ return bits(r_, 42); }
unsigned scr::sd_specx() const			{ return bits(r_, 38, 41); }
unsigned scr::cmd_support() const		{ return bits(r_, 32, 35); }

/*
 * card_status
 */
card_status::card_status()
{ }

card_status::card_status(void *p)
{
	memcpy(&r_, p, 4);
	r_ = be32toh(r_);
}

bool card_status::out_of_range() const		{ return bits(r_, 31); }
bool card_status::address_error() const		{ return bits(r_, 30); }
bool card_status::block_len_error() const	{ return bits(r_, 29); }
bool card_status::erase_seq_error() const	{ return bits(r_, 28); }
bool card_status::erase_param() const		{ return bits(r_, 27); }
bool card_status::wp_violation() const		{ return bits(r_, 26); }
bool card_status::card_is_locked() const	{ return bits(r_, 25); }
bool card_status::lock_unlock_failed() const	{ return bits(r_, 24); }
bool card_status::com_crc_error() const		{ return bits(r_, 23); }
bool card_status::illegal_command() const	{ return bits(r_, 22); }
bool card_status::card_ecc_failed() const	{ return bits(r_, 21); }
bool card_status::cc_error() const		{ return bits(r_, 20); }
bool card_status::error() const			{ return bits(r_, 19); }
bool card_status::deferred_response() const	{ return bits(r_, 17); }
bool card_status::csd_overwrite() const		{ return bits(r_, 16); }
bool card_status::wp_erase_skip() const		{ return bits(r_, 15); }
bool card_status::card_ecc_disabled() const	{ return bits(r_, 14); }
bool card_status::erase_reset() const		{ return bits(r_, 13); }
unsigned card_status::current_state() const	{ return bits(r_, 9, 12); }
bool card_status::ready_for_data() const	{ return bits(r_, 8); }
bool card_status::fx_event() const		{ return bits(r_, 6); }
bool card_status::app_cmd() const		{ return bits(r_, 5); }
bool card_status::ake_seq_error() const		{ return bits(r_, 3); }

bool
card_status::any_error() const
{
	/*
	 * Ideally we would write something along the lines of:
	 *
	 * return out_of_range() || address_error() || block_len_error() ||
	 *     erase_seq_error() || erase_param() || wp_violation() ||
	 *     lock_unlock_failed() || com_crc_error() || illegal_command() ||
	 *     card_ecc_failed() || cc_error() || error() || csd_overwrite() ||
	 *     wp_erase_skip() || ake_seq_error();
	 *
	 * Which is equivalent to the below, but GCC 8.3.0 optimises that badly.
	 * Maybe we can try again in a future version.
	 */
	return r_ & 0b11111101111110011000000000001000;
}

/*
 * ddr_mode
 */
bool
ddr_mode(access_mode m)
{
	return m == access_mode::ddr50;
}

/*
 * to_string
 */
const char *
to_string(access_mode m)
{
	switch (m) {
	case access_mode::default_sdr12: return "SDR12";
	case access_mode::high_sdr25: return "SDR25";
	case access_mode::sdr50: return "SDR50";
	case access_mode::sdr104: return "SDR104";
	case access_mode::ddr50: return "DDR50";
	default: __builtin_unreachable();
	};
}

/*
 * output_impedance
 */
unsigned
output_impedance(driver_strength v)
{
	switch (v) {
	case driver_strength::type_b_50_ohm: return 50;
	case driver_strength::type_a_33_ohm: return 33;
	case driver_strength::type_c_66_ohm: return 66;
	case driver_strength::type_d_100_ohm: return 100;
	default: __builtin_unreachable();
	};
}

/*
 * function_status
 */
function_status::function_status()
{ }

void *
function_status::data()
{
	return r_.data();
}

size_t
function_status::size() const
{
	return r_.size();
}

unsigned
function_status::max_power() const			{ return bits(r_, 496, 511); }

unsigned
function_status::function_6_support() const		{ return bits(r_, 480, 495); }

unsigned
function_status::function_5_support() const		{ return bits(r_, 464, 479); }

bitfield<power_limit>
function_status::power_limit() const			{ return bits(r_, 448, 463); }

bitfield<driver_strength>
function_status::driver_strength() const		{ return bits(r_, 432, 447); }

unsigned
function_status::function_2_support() const		{ return bits(r_, 416, 431); }

bitfield<access_mode>
function_status::access_mode() const			{ return bits(r_, 400, 415); }

unsigned
function_status::function_6_selection() const		{ return bits(r_, 396, 399); }

unsigned
function_status::function_5_selection() const		{ return bits(r_, 392, 395); }

unsigned
function_status::power_limit_selection() const		{ return bits(r_, 388, 391); }

unsigned
function_status::driver_strength_selection() const	{ return bits(r_, 384, 387); }

unsigned
function_status::command_system_selection() const	{ return bits(r_, 380, 383); }

unsigned
function_status::access_mode_selection() const		{ return bits(r_, 376, 379); }

unsigned
function_status::version() const			{ return bits(r_, 368, 375); }

/*
 * status
 */
status::status()
{ }

void *
status::data()
{
	return r_.data();
}

size_t
status::size() const
{
	return r_.size();
}

unsigned status::dat_bus_width() const		       { return bits(r_, 510, 511); }
bool status::secured_mode() const		       { return bits(r_, 509); }
unsigned status::sd_card_type() const		       { return bits(r_, 480, 495); }
unsigned long status::size_of_protected_area() const   { return bits(r_, 448, 479); }
unsigned status::speed_class() const		       { return bits(r_, 440, 447); }
unsigned status::performance_move() const	       { return bits(r_, 432, 439); }
unsigned status::au_size() const		       { return bits(r_, 428, 431); }
unsigned status::erase_size() const		       { return bits(r_, 408, 423); }
unsigned status::erase_timeout() const		       { return bits(r_, 402, 407); }
unsigned status::erase_offset() const		       { return bits(r_, 400, 401); }
unsigned status::uhs_speed_grade() const	       { return bits(r_, 396, 399); }
unsigned status::uhs_au_size() const		       { return bits(r_, 392, 395); }
unsigned status::video_speed_class() const	       { return bits(r_, 384, 391); }
unsigned status::vsc_au_size() const		       { return bits(r_, 368, 377); }
unsigned long status::sus_addr() const		       { return bits(r_, 346, 367); }
unsigned status::app_perf_class() const		       { return bits(r_, 336, 339); }
unsigned status::performance_enhance() const	       { return bits(r_, 328, 335); }
bool status::discard_support() const		       { return bits(r_, 313); }
bool status::fule_support() const		       { return bits(r_, 312); }

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
 * send_if_cond
 */
int
send_if_cond(host *h, const float io_v)
{
	const uint32_t check_pattern = 0x5a;
	uint32_t vhs;

	/* REVISIT: LV initialisation not yet defined. */
	if (io_v >= 2.7 && io_v <= 3.6)
		vhs = 1;
	else
		return DERR(-EINVAL);

	command cmd{8, vhs << 8 | check_pattern, command::response_type::r7};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	/* Response bits are documented with crc but crc has been stripped. */
	if (bits(cmd.response().data(), 4, 8 - 8, 15 - 8) != check_pattern)
		return DERR(-EIO);
	if (bits(cmd.response().data(), 4, 16 - 8, 19 - 8) != vhs)
		return DERR(-ENOTSUP);

	return 0;
}

/*
 * sd_send_op_cond
 */
int
sd_send_op_cond(host *h, bool s18r, float supply_v, ocr &o)
{
	/* REVISIT: LV initialisation not yet defined. */
	if (supply_v != 0 && (supply_v < 2.7 || supply_v > 3.6))
		return DERR(-EINVAL);

	const uint32_t hcs = 1;	/* support high capacity cards */
	const uint32_t xpc = 1;	/* support > 150mA operation */
	const uint32_t voltage_window = supply_v == 0
	    ? 0 : 0x80ul << ((int)(supply_v * 10) - 27);
	command cmd{41 | command::ACMD,
	    hcs << 30 | xpc << 28 | s18r << 24 | voltage_window << 8,
	    command::response_type::r3};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	o = ocr{cmd.response().data()};

	return 0;
}

/*
 * voltage_swich
 */
int
voltage_switch(host *h)
{
	command cmd{11, 0, command::response_type::r1};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	if (card_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

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
 * send_relative_addr
 */
int
send_relative_addr(host *h, unsigned &rca)
{
	command cmd{3, 0, command::response_type::r6};

	if (auto r = h->run_command(cmd, 0); r < 0)
		return r;

	/* Response bits are documented with crc but crc has been stripped. */
	rca = bits(cmd.response().data(), 4, 24 - 8, 39 - 8);

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

	card_status s{cmd.response().data()};

	if (s.any_error())
		return DERR(-EIO);
	if (s.card_is_locked())
		return DERR(-EACCES);

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
 * send_scr
 */
int
send_scr(host *h, unsigned rca, scr &s)
{
	iovec iov{s.data(), s.size()};
	command cmd{51 | command::ACMD, 0, command::response_type::r1};
	cmd.setup_data_transfer(command::data_direction::device_to_host,
	    s.size(), &iov, 0, s.size());

	if (auto r = h->run_command(cmd, rca); (size_t)r != s.size())
		return r < 0 ? r : DERR(-EIO);

	if (card_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	return 0;
}

/*
 * set_bus_width
 */
int
set_bus_width(host *h, unsigned rca, unsigned w)
{
	if (w != 1 && w != 4)
		return DERR(-EINVAL);

	command cmd{6 | command::ACMD, w / 2, command::response_type::r1};

	if (auto r = h->run_command(cmd, rca); r < 0)
		return r;

	if (card_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	return 0;
}

/*
 * check_func
 */
int
check_func(host *h, function_status &f)
{
	iovec iov{f.data(), f.size()};
	command cmd{6, 0, command::response_type::r1};
	cmd.setup_data_transfer(command::data_direction::device_to_host,
	    f.size(), &iov, 0, f.size());

	if (auto r = h->run_command(cmd, 0); (size_t)r != f.size())
		return r < 0 ? r : DERR(-EIO);

	if (card_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	return 0;
}

/*
 * switch_func
 */
int
switch_func(host *h, power_limit p, driver_strength d, access_mode a)
{
	function_status f;

	const uint32_t mode = 1;
	const uint32_t power_limit = static_cast<uint32_t>(p);
	const uint32_t driver_strength = static_cast<uint32_t>(d);
	const uint32_t command_system = 0;
	const uint32_t access_mode = static_cast<uint32_t>(a);
	iovec iov{f.data(), f.size()};
	command cmd{6,
	    mode << 31 | power_limit << 12 | driver_strength << 8 |
	    command_system << 4 | access_mode,
	    command::response_type::r1};
	cmd.setup_data_transfer(command::data_direction::device_to_host,
	    f.size(), &iov, 0, f.size());

	if (auto r = h->run_command(cmd, 0); (size_t)r != f.size())
		return r < 0 ? r : DERR(-EIO);

	if (card_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

	if (f.access_mode_selection() != access_mode ||
	    f.command_system_selection() != command_system ||
	    f.driver_strength_selection() != driver_strength ||
	    f.access_mode_selection() != access_mode)
		return DERR(-EIO);

	return 0;
}

/*
 * sd_status
 */
int
sd_status(host *h, unsigned rca, status &s)
{
	iovec iov{s.data(), s.size()};
	command cmd{13 | command::ACMD, 0, command::response_type::r1};
	cmd.setup_data_transfer(command::data_direction::device_to_host,
	    s.size(), &iov, 0, s.size());

	if (auto r = h->run_command(cmd, rca); (size_t)r != s.size())
		return r < 0 ? r : DERR(-EIO);

	if (card_status{cmd.response().data()}.any_error())
		return DERR(-EIO);

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

	if (card_status{cmd.response().data()}.any_error())
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

}
