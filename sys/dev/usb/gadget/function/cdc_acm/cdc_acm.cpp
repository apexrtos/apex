#include "cdc_acm.h"

#include <debug.h>
#include <dev/tty/tty.h>
#include <dev/usb/gadget/descriptors.h>
#include <dev/usb/gadget/device.h>
#include <dev/usb/gadget/transaction.h>
#include <dev/usb/gadget/udc.h>
#include <endian.h>
#include <errno.h>
#include <mutex>
#include <string_utils.h>
#include <termios.h>

#define trace(...)

namespace usb::gadget {

namespace {

inline constexpr auto cdc_acm_interrupt_packet_len = 16;
inline constexpr auto rx_txn = 2;
inline constexpr auto tx_txn = 2;

/*
 * oproc - Called whenever UART output should start.
 */
void
oproc(tty *t)
{
	auto p = reinterpret_cast<cdc_acm *>(tty_data(t));
	p->tx_queue();
}

/*
 * iproc - Called whenever UART input should start.
 */
void
iproc(tty *t)
{
	auto p = reinterpret_cast<cdc_acm *>(tty_data(t));
	p->rx_queue();
}

/*
 * fproc - Called to flush UART input, output or both
 */
void
fproc(tty *t, int io)
{
	auto p = reinterpret_cast<cdc_acm *>(tty_data(t));
	if (io == TCIFLUSH || io == TCIOFLUSH)
		p->flush_input();
	if (io == TCOFLUSH || io == TCIOFLUSH)
		p->flush_output();
}

}

/*
 * cdc_acm::cdc_acm
 */
cdc_acm::cdc_acm(gadget::udc &u)
: function{u, 3, 2}
, t_{nullptr}
, running_{false}
, line_coding_{115200, 0, 0, 8}
{
	tx_.reserve(tx_txn);
	for (size_t i{0}; i != tx_txn; ++i) {
		tx_.emplace_back(udc().alloc_transaction());
		tx_.back()->on_done([this](transaction *t, int status) {
			tx_done(t, status);
		});
	}

	rx_.reserve(rx_txn);
	for (size_t i{0}; i != rx_txn; ++i) {
		rx_.emplace_back(udc().alloc_transaction());
		rx_.back()->on_done([this](transaction *t, int status) {
			rx_done(t, status);
		});
	}
}

/*
 * cdc_acm::~cdc_acm
 */
cdc_acm::~cdc_acm()
{ }

/*
 * cdc_acm::tx_queue - queue tty data for transmision over USB
 */
void
cdc_acm::tx_queue()
{
	std::lock_guard l{lock_};

	if (!running_)
		return;

	while (!tx_.empty()) {
		const void *p;
		const auto len = tty_tx_getbuf(t_,
		    bulk_max_packet_len(Speed::High), &p);
		if (!len)
			return;
		auto &t = tx_.back();
		t->set_buf(p, len);
		if (auto r = udc().queue(endpoint_offset() + 1,
		    ch9::Direction::DeviceToHost, t.get()); r < 0)
			break;
		t.release();
		tx_.pop_back();
	}
}

/*
 * cdc_acm::rx_queue - queue tty buffers for reception over USB
 */
void
cdc_acm::rx_queue()
{
	std::lock_guard l{lock_};

	if (!running_)
		return;

	while (!rx_.empty()) {
		void *p = tty_rx_getbuf(t_);
		if (!p)
			return;
		auto &t = rx_.back();
		t->set_buf(p, bulk_max_packet_len(Speed::High));
		if (auto r = udc().queue(endpoint_offset() + 1,
		    ch9::Direction::HostToDevice, t.get()); r < 0)
			break;
		t.release();
		rx_.pop_back();
	}
}

/*
 * cdc_acm::flush_output - flush data from tx queue
 */
void
cdc_acm::flush_output()
{
	udc().flush(endpoint_offset() + 1, ch9::Direction::DeviceToHost);
}

/*
 * cdc_acm::flush_input - flush data from rx queue
 */
void
cdc_acm::flush_input()
{
	udc().flush(endpoint_offset() + 1, ch9::Direction::HostToDevice);
}

/*
 * cdc_acm::v_configure - configure cdc_acm instance
 */
int
cdc_acm::v_configure(std::string_view c)
{
	auto r = parse_options(c, [this](const auto &name, const auto &value) {
		if (value.empty())
			return DERR(-EINVAL);
		if (name == "dev") {
			if (t_)
				return DERR(-EINVAL);
			/* REVISIT: converting the device name to a std::string
			 * here is pretty horrible, but it's a way to match the
			 * tty_create interface for now. */
			const auto t = tty_create(std::string(value).c_str(),
			    bulk_max_packet_len(Speed::High), rx_txn, nullptr,
			    oproc, iproc, fproc, this);
			if (t > (void*)-4096UL)
				return (int)t;
			t_ = t;
		} else if (name == "function")
			function_ = value;
		return 0;
	});
	if (r < 0)
		return r;
	if (!t_)
		return DERR(-EINVAL);
	return 0;
}

/*
 * cdc_acm::v_init - initialise cdc_acm instance
 */
int
cdc_acm::v_init(device &d)
{
	d.add_string(function_);
	return 0;
}

/*
 * cdc_acm::v_finalise - finalise cdc_acm instance
 */
int
cdc_acm::v_finalise()
{
	std::lock_guard l{lock_};

	if (running_)
		return DERR(-EBUSY);

	if (!t_)
		return 0;

	const auto r = tty_destroy(t_);
	t_ = nullptr;

	return r;
}

/*
 * cdc_acm::v_reset - reset cdc_acm instance
 */
void
cdc_acm::v_reset()
{
	v_stop();
}

/*
 * cdc_acm::v_start - start cdc_acm instance
 */
int
cdc_acm::v_start(const Speed spd)
{
	if (!t_)
		return DERR(-EILSEQ);

	if (spd == Speed::Low)
		return 0;

	const auto eo = endpoint_offset();
	if (auto r{udc().open_endpoint(eo + 0, ch9::Direction::DeviceToHost,
	    ch9::TransferType::Interrupt, cdc_acm_interrupt_packet_len)}; r < 0)
		return r;
	if (auto r{udc().open_endpoint(eo + 1, ch9::Direction::DeviceToHost,
	    ch9::TransferType::Bulk, bulk_max_packet_len(spd))}; r < 0)
		return r;
	if (auto r{udc().open_endpoint(eo + 1, ch9::Direction::HostToDevice,
	    ch9::TransferType::Bulk, bulk_max_packet_len(spd))}; r < 0)
		return r;

	std::unique_lock l{lock_};
	running_ = true;
	l.unlock();

	rx_queue();
	tx_queue();

	return 0;
}

/*
 * cdc_acm::v_stop - stop cdc_acm instance
 */
void
cdc_acm::v_stop()
{
	std::unique_lock l{lock_};
	running_ = false;
	l.unlock();

	const auto eo = endpoint_offset();
	udc().close_endpoint(eo + 0, ch9::Direction::DeviceToHost);
	udc().close_endpoint(eo + 1, ch9::Direction::DeviceToHost);
	udc().close_endpoint(eo + 1, ch9::Direction::HostToDevice);
}

/*
 * cdc_acm::v_process_setup - attempt to handle a setup request
 */
setup_result
cdc_acm::v_process_setup(const setup_request &s, transaction &t)
{
	if (request_type(s) != ch9::RequestType::Class)
		return setup_result::error;
	/* Even though our capabilities advertise that we do not support these
	 * request, certain Windows software ignores this advice and uses them
	 * anyway, treating failure as fatal. */
	switch (static_cast<cdc::Request>(s.request())) {
	case cdc::Request::SET_LINE_CODING:
		if (s.length() != sizeof(line_coding_)) {
			trace("cdc_acm::v_process_setup SET_LINE_CODING bad "
			    "length %zu\n", s.length());
			return setup_result::error;
		}
		t.on_done([this](transaction *t, int status) {
			trace("cdc_acm::SET_LINE_CODING status %d\n", status);
			if (status < 0)
				return;
			memcpy(&line_coding_, t->buf(), sizeof(line_coding_));
		});
		return setup_result::data;
	case cdc::Request::GET_LINE_CODING:
		if (s.length() != sizeof(cdc::pstn::line_coding)) {
			trace("cdc_acm::v_process_setup GET_LINE_CODING bad "
			    "length %zu\n", s.length());
			return setup_result::error;
		}
		t.set_buf(&line_coding_, sizeof(line_coding_));
		return setup_result::data;
	case cdc::Request::SET_CONTROL_LINE_STATE:
		trace("cdc_acm::SET_CONTROL_LINE_STATE %x\n", s.value());
		return setup_result::status;
	default:
		trace("cdc_acm::v_process_setup: %d not supported\n",
		    s.request());
		return setup_result::error;
	}
}

/*
 * cdc_acm::v_sizeof_descriptors - return size of descriptors
 */
size_t
cdc_acm::v_sizeof_descriptors(const Speed spd) const
{
	switch (spd) {
	case Speed::Low:
		return 0;
	case Speed::High:
	case Speed::Full:
		return
		    sizeof(ch9::interface_association_descriptor) +
		    sizeof(cdc::header_functional_descriptor) +
		    sizeof(cdc::union_functional_descriptor) + 1 +
		    sizeof(cdc::pstn::call_management_functional_descriptor) +
		    sizeof(cdc::pstn::abstract_control_management_functional_descriptor) +
		    sizeof(ch9::interface_descriptor) +
		    sizeof(ch9::endpoint_descriptor) +
		    sizeof(ch9::interface_descriptor) +
		    sizeof(ch9::endpoint_descriptor) +
		    sizeof(ch9::endpoint_descriptor);
	}

	__builtin_unreachable();
}

/*
 * cdc_acm::v_write_descriptors - write descriptors
 */
size_t
cdc_acm::v_write_descriptors(const Speed spd, std::span<std::byte> m)
{
	if (spd == Speed::Low)
		return 0;

	auto pos = begin(m);
	auto write_descriptor = [&](const auto &v){
		const auto len = std::min<size_t>(sizeof(v), end(m) - pos);
		memcpy(&pos[0], &v, len);
		pos += len;
	};

	write_descriptor(interface_association_descriptor(
	    interface_offset(), 2,
	    Class::CDC,
	    cdc::SubClass::Abstract_Control_Model,
	    cdc::Protocol::Not_Required,
	    function_.index()));
	write_descriptor(interface_descriptor(
	    interface_offset(), 0, 1,
	    Class::CDC,
	    static_cast<uint8_t>(cdc::SubClass::Abstract_Control_Model),
	    static_cast<uint8_t>(cdc::Protocol::Not_Required),
	    function_.index()));

	cdc::header_functional_descriptor hf{};
	hf.bFunctionLength = sizeof(hf);
	hf.bDescriptorType = ch9::DescriptorType::CS_INTERFACE;
	hf.bDescriptorSubtype = cdc::Function::Header;
	hf.bcdCDC = htole16(cdc::version);
	write_descriptor(hf);

	struct {
		cdc::union_functional_descriptor d;
		uint8_t bSubordinateInterface;
	} __attribute__((packed)) uf{};
	uf.d.bFunctionLength = sizeof(uf);
	uf.d.bDescriptorType = ch9::DescriptorType::CS_INTERFACE;
	uf.d.bDescriptorSubtype = cdc::Function::Union;
	uf.d.bControlInterface = interface_offset();
	uf.bSubordinateInterface = interface_offset() + 1;
	write_descriptor(uf);

	cdc::pstn::call_management_functional_descriptor cmf{};
	cmf.bFunctionLength = sizeof(cmf);
	cmf.bDescriptorType = ch9::DescriptorType::CS_INTERFACE;
	cmf.bDescriptorSubtype = cdc::Function::Call_Management;
	cmf.bmCapabilities = 0;
	cmf.bDataInterface = interface_offset() + 1;
	write_descriptor(cmf);

	cdc::pstn::abstract_control_management_functional_descriptor acmf{};
	acmf.bFunctionLength = sizeof(acmf);
	acmf.bDescriptorType = ch9::DescriptorType::CS_INTERFACE;
	acmf.bDescriptorSubtype = cdc::Function::Abstract_Control_Management;
	acmf.bmCapabilities = 0;
	write_descriptor(acmf);

	/* Notifications not currently used, so poll as slowly as possible. */
	write_descriptor(interrupt_endpoint_descriptor(
	    endpoint_offset(), ch9::Direction::DeviceToHost,
	    cdc_acm_interrupt_packet_len,
	    spd == Speed::Full ? 255 : 16));
	write_descriptor(interface_descriptor(
	    interface_offset() + 1, 0, 2,
	    Class::CDC_Data, 0, 0,
	    function_.index()));
	write_descriptor(bulk_endpoint_descriptor(
	    endpoint_offset() + 1, ch9::Direction::DeviceToHost,
	    bulk_max_packet_len(spd)));
	write_descriptor(bulk_endpoint_descriptor(
	    endpoint_offset() + 1, ch9::Direction::HostToDevice,
	    bulk_max_packet_len(spd)));

	return pos - begin(m);
}

/*
 * cdc_acm::rx_done - USB receive transaction retired
 */
void
cdc_acm::rx_done(transaction *t, int status)
{
	/* tty must always be informed that buffer is no longer required, even
	 * if the transaction failed for some reason */
	tty_rx_putbuf(t_, static_cast<char *>(t->buf()),
	    status < 0 ? 0 : status);

	std::unique_lock l{lock_};
	rx_.emplace_back(t);
	l.unlock();

	if (status > 0)
		rx_queue();
}

/*
 * cdc_acm::tx_done - USB transmit transaction retired
 */
void
cdc_acm::tx_done(transaction *t, int status)
{
	/* tty must always be informed that buffer is no longer required, even
	 * if the transaction failed for some reason */
	tty_tx_advance(t_, t->len());

	std::unique_lock l{lock_};
	tx_.emplace_back(t);
	l.unlock();

	if (status > 0)
		tx_queue();
}

}
