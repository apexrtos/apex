#include "udc.h"

#include "transaction.h"
#include <cerrno>
#include <cstring>
#include <debug.h>
#include <dma.h>
#include <irq.h>
#include <list>
#include <mutex>
#include <sch.h>
#include <thread.h>

#define trace(...)

namespace usb::gadget {

namespace {

/*
 * events for event_ bitfield
 */
inline constexpr auto reset_event = 0x1;
inline constexpr auto bus_reset_event = 0x2;
inline constexpr auto port_change_event = 0x4;
inline constexpr auto setup_request_event = 0x8;
inline constexpr auto setup_aborted_event = 0x10;
inline constexpr auto ep_complete_event = 0x20;

/*
 * opposite - return opposite direction
 */
ch9::Direction
opposite(ch9::Direction d)
{
	if (d == ch9::Direction::HostToDevice)
		return ch9::Direction::DeviceToHost;
	else
		return ch9::Direction::HostToDevice;
}

}

/*
 * udc::udc
 */
udc::udc(std::string_view name, const size_t endpoints)
: name_{name}
, endpoints_{endpoints}
, running_{false}
, state_{ch9::DeviceState::Detached}
, events_{0}
, complete_{0}
, attached_irq_{false}
, speed_irq_{Speed::High}
, setup_buf_{nullptr}
{
	assert(endpoints_ <= 16);

	if (!(th_ = kthread_create(&th_fn_wrapper, this, PRI_DPC, "udc",
				   MA_NORMAL)))
		panic("OOM");
}

/*
 * udc::~udc
 */
udc::~udc()
{
	thread_terminate(th_);
}

/*
 * udc::start - start USB device controller
 */
int
udc::start()
{
	std::lock_guard l{lock_};

	if (running_)
		return 0;
	if (!device_)
		return DERR(-EINVAL);
	if (!txn_ && !(txn_ = alloc_transaction()))
		return DERR(-ENOMEM);
	if (!setup_buf_ && !(setup_buf_ = dma_alloc(setup_buf_sz_)))
		return DERR(-ENOMEM);
	if (auto r = device_->init(); r < 0)
		return r;
	if (device_->max_endpoints() >= endpoints_)
		return DERR(-ERANGE);
	if (auto r = v_start(); r < 0)
		return r;
	running_ = true;
	return 0;
}

/*
 * udc::stop - stop USB device controller
 */
void
udc::stop()
{
	std::lock_guard l{lock_};

	if (!running_)
		return;
	running_ = false;
	device_->reset();
	v_stop();
	state_ = ch9::DeviceState::Detached;
	events_ = 0;
	complete_ = 0;
	speed_irq_ = Speed::High;
}

/*
 * udc::state - retrieve device state
 */
ch9::DeviceState
udc::state()
{
	std::lock_guard l{lock_};

	return state_;
}

/*
 * udc::set_device - set device implementation
 */
int
udc::set_device(std::unique_ptr<gadget::device> d)
{
	std::unique_lock l{lock_};
	if (running_)
		return DERR(-EBUSY);

	/* Storing the old device in a temporary means that it won't be
	 * destroyed until after lock_ is released. This is necessary as some
	 * devices need to sleep in order to terminate active operations. */
	auto tmp = std::move(device_);
	device_ = std::move(d);
	l.unlock();

	return 0;
}

/*
 * udc::device - retrieve device implementation
 */
std::shared_ptr<gadget::device>
udc::device()
{
	std::lock_guard l{lock_};

	return device_;
}

/*
 * udc::name
 */
const std::string&
udc::name() const
{
	return name_;
}

/*
 * udc::endpoints
 */
size_t
udc::endpoints() const
{
	return endpoints_;
}

/*
 * udc::open_endpoint - open an endpoint on the device controller
 */
int
udc::open_endpoint(size_t endpoint, ch9::Direction dir, ch9::TransferType tt,
    size_t max_packet_len)
{
	if (endpoint == 0)
		return DERR(-EINVAL);

	return v_open_endpoint(endpoint, dir, tt, max_packet_len);
}

/*
 * udc::close_endpoint - close an endpoint on the device controller
 */
void
udc::close_endpoint(size_t endpoint, ch9::Direction dir)
{
	if (endpoint == 0)
		return;

	v_close_endpoint(endpoint, dir);
}

/*
 * udc::queue - queue a transaction onto an endpoint
 */
int
udc::queue(size_t endpoint, ch9::Direction dir, transaction *t)
{
	if (endpoint == 0)
		return DERR(-EINVAL);

	return v_queue(endpoint, dir, t);
}

/*
 * udc::flush - flush an endpoint
 */
int
udc::flush(size_t endpoint, ch9::Direction dir)
{
	if (endpoint == 0)
		return DERR(-EINVAL);

	return v_flush(endpoint, dir);
}

/*
 * udc::alloc_transaction - allocate a transaction object suitable for use
 *			    with this device controller
 */
std::unique_ptr<transaction>
udc::alloc_transaction()
{
	return v_alloc_transaction();
}

/*
 * udc::reset_irq - USB controller required reset
 *
 * Calls v_reset in thread context for futher processing.
 * Calls device_->reset().
 * See process_events for further details.
 *
 * Interrupt safe.
 */
void
udc::reset_irq()
{
	events_ |= reset_event;
	semaphore_.post_once();
}

/*
 * udc::bus_reset_irq - USB bus reset received
 *
 * Calls v_bus_reset in thread context for further processing.
 * Calls device_->reset().
 * See process_events for further details.
 *
 * Interrupt safe.
 */
void
udc::bus_reset_irq()
{
	events_ |= bus_reset_event;
	semaphore_.post_once();
}

/*
 * udc::port_change_irq - USB connection detected
 *
 * Calls v_port_change in thread context for further processing.
 * Calls device_->reset() on disconnection.
 * See process_events for further details.
 *
 * Interrupt safe.
 */
void
udc::port_change_irq(bool connected, Speed s)
{
	attached_irq_ = connected;
	speed_irq_ = s;

	events_ |= port_change_event;
	semaphore_.post_once();
}

/*
 * udc::setup_request_irq - USB setup request received
 *
 * See process_events for further details.
 *
 * Interrupt safe.
 */
void
udc::setup_request_irq(size_t endpoint, const ch9::setup_data &s)
{
	/* only default control pipe (endpoint 0) supported */
	if (endpoint != 0) {
		dbg("udc::setup_request_irq: ignoring setup on endpoint %d\n",
		    endpoint);
		return;
	}

	setup_data_irq_ = s;
	events_ |= setup_request_event;
	semaphore_.post_once();
}

/*
 * udc::setup_aborted_irq - USB setup request aborted
 *
 * Calls v_setup_aborted in thread context for further processing.
 * See process_events for further details.
 *
 * Interrupt safe.
 */
void
udc::setup_aborted_irq(size_t endpoint)
{
	/* only default control pipe (endpoint 0) supported */
	if (endpoint != 0) {
		dbg("udc::setup_aborted_irq: ignoring abort on endpoint %d\n",
		    endpoint);
		return;
	}

	events_ |= setup_aborted_event;
	semaphore_.post_once();
}

/*
 * udc::ep_complete_irq - USB transaction completed
 *
 * Calls v_complete in thread context for further processing.
 * See process_events for further details.
 *
 * Interrupt safe.
 */
void
udc::ep_complete_irq(size_t endpoint, ch9::Direction dir)
{
	trace("udc::ep_complete_irq ep:%zu dir %d\n",
	    endpoint, static_cast<int>(dir));

	events_ |= ep_complete_event;
	complete_ |= 1UL << (endpoint * 2 + static_cast<int>(dir));
	semaphore_.post_once();
}

/*
 * udc::setup_requested
 *
 * Returns true if a setup request has been received.
 */
bool
udc::setup_requested(size_t endpoint)
{
	/* only default control pipe (endpoint 0) supported */
	assert(endpoint == 0);

	return events_ & setup_request_event;
}

/*
 * udc::queue_setup - queue setup transaction on endpoint 0
 *
 * Caller must hold lock_.
 */
int
udc::queue_setup(const ch9::Direction dir, transaction *t)
{
	lock_.assert_locked();

	int ret = 0;

	if ((ret = v_queue_setup(0, dir, t)) < 0)
		v_set_stall(0, true);

	return ret;
}

/*
 * udc::process_setup - process USB setup request
 *
 * Caller must hold lock_.
 */
void
udc::process_setup(const setup_request &s, const Speed spd)
{
	lock_.assert_locked();

	if (txn_->running()) {
		warning("udc::setup_request: BUG transaction in progress!\n");
		v_set_stall(0, true);
		return;
	}
	txn_->clear();

	auto dir = request_direction(s);
	switch (dispatch_setup_request(s, spd)) {
	case setup_result::error: {
		dbg("udc::setup_request: bmRequestType %x bRequest %d "
		    "wValue %d wIndex %d wLength %d not supported!\n",
		    s.request_type(), s.request(), s.value(), s.index(),
		    s.length());
		v_set_stall(0, true);
		break;
	}
	case setup_result::data:
		if (dir == ch9::Direction::HostToDevice && !txn_->buf()) {
			if (s.length() > setup_buf_sz_) {
				dbg("udc::setup_request: bmRequestType %x "
				    "bRequest %d wValue %d wIndex %d "
				    "wLength %d too big!\n",
				    s.request_type(), s.request(), s.value(),
				    s.index(), s.length());
				v_set_stall(0, true);
				break;
			}
			txn_->set_buf(setup_buf_, s.length());
		}
		txn_->on_finalise([this, dir](transaction *t, int status) {
			if (status < 0)
				return;
			t->clear();
			queue_setup(opposite(dir), t);
		});
		queue_setup(dir, txn_.get());
		break;
	case setup_result::status:
		queue_setup(opposite(dir), txn_.get());
		break;
	case setup_result::complete:
		break;
	}
}

/*
 * udc::dispatch_setup_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::dispatch_setup_request(const setup_request &s, const Speed spd)
{
	lock_.assert_locked();

	/* see if device can handle request */
	if (auto r = device_->process_setup(s, spd, *txn_);
	    r != setup_result::error) {
		/* track configuration state */
		if (request_type(s) == ch9::RequestType::Standard &&
		    request_direction(s) == ch9::Direction::HostToDevice &&
		    request_recipient(s) == ch9::RequestRecipient::Device &&
		    standard_request(s) == ch9::Request::SET_CONFIGURATION)
			state_ = configuration(s)
			    ? ch9::DeviceState::Configured
			    : ch9::DeviceState::Address;
		return r;
	}

	/* handle some standard requests here */
	if (request_type(s) != ch9::RequestType::Standard)
		return setup_result::error;
	switch (request_recipient(s)) {
	case ch9::RequestRecipient::Device:
		return device_request(s);
	case ch9::RequestRecipient::Interface:
		return interface_request(s);
	case ch9::RequestRecipient::Endpoint:
		return endpoint_request(s);
	default:
		return setup_result::error;
	}
}

/*
 * udc::device_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::device_request(const setup_request &s)
{
	lock_.assert_locked();

	switch (standard_request(s)) {
	case ch9::Request::GET_STATUS:
		return device_get_status_request(s);
	case ch9::Request::CLEAR_FEATURE:
		return device_feature_request(s, false);
	case ch9::Request::SET_FEATURE:
		return device_feature_request(s, true);
	case ch9::Request::SET_ADDRESS:
		return device_set_address_request(s);
	default:
		return setup_result::error;
	}
}

/*
 * udc::interface_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::interface_request(const setup_request &s)
{
	lock_.assert_locked();

	switch (standard_request(s)) {
	case ch9::Request::GET_STATUS:
		return interface_get_status_request(s);
	default:
		return setup_result::error;
	}
}

/*
 * udc::endpoint_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::endpoint_request(const setup_request &s)
{
	lock_.assert_locked();

	switch (standard_request(s)) {
	case ch9::Request::GET_STATUS:
		return endpoint_get_status_request(s);
	case ch9::Request::CLEAR_FEATURE:
		return endpoint_feature_request(s, false);
	case ch9::Request::SET_FEATURE:
		return endpoint_feature_request(s, true);
	default:
		return setup_result::error;
	}
}

/*
 * udc::device_get_status_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::device_get_status_request(const setup_request &s)
{
	lock_.assert_locked();

	uint16_t status = 0;

	if (request_direction(s) != ch9::Direction::DeviceToHost)
		return setup_result::error;
	if (s.length() != sizeof(status))
		return setup_result::error;
/* TODO: implement power status & remote wakeup
	if (self_powered_)
		status |= ch9::DeviceStatus::SelfPowered;
	if (remote_wakeup_)
		status |= ch9::DeviceStatus::RemoteWakeup;
*/
	status = htole16(status);
	memcpy(setup_buf_, &status, sizeof(status));
	txn_->set_buf(setup_buf_, sizeof(status));
	return setup_result::data;
}

/*
 * udc::device_feature_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::device_feature_request(const setup_request &s, bool set)
{
	lock_.assert_locked();

	dbg("udc::device_feature_reqeust: not yet supported\n");
	return setup_result::error;
}

/*
 * udc::device_set_address_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::device_set_address_request(const setup_request &s)
{
	lock_.assert_locked();

	if (request_direction(s) != ch9::Direction::HostToDevice)
		return setup_result::error;
	if (state_ == ch9::DeviceState::Configured)
		return setup_result::error;
	txn_->on_done([this, addr = address(s)](transaction *t, int status) {
		if (status < 0)
			return;
		v_set_address(addr);
		if (addr)
			state_ = ch9::DeviceState::Address;
		else
			state_ = ch9::DeviceState::Default;
	});
	queue_setup(ch9::Direction::DeviceToHost, txn_.get());
	return setup_result::complete;
}

/*
 * udc::interface_get_status_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::interface_get_status_request(const setup_request &s)
{
	lock_.assert_locked();

	uint16_t status = 0;

	if (request_direction(s) != ch9::Direction::DeviceToHost)
		return setup_result::error;
	if (interface(s) >= device_->active_interfaces())
		return setup_result::error;
	if (s.length() != sizeof(status))
		return setup_result::error;
	memcpy(setup_buf_, &status, sizeof(status));
	txn_->set_buf(setup_buf_, sizeof(status));
	return setup_result::data;
}

/*
 * udc::endpoint_get_status_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::endpoint_get_status_request(const setup_request &s)
{
	lock_.assert_locked();

	uint16_t status = 0;

	if (request_direction(s) != ch9::Direction::DeviceToHost)
		return setup_result::error;
	if (endpoint(s) >= endpoints_)
		return setup_result::error;
	if (s.length() != sizeof(status))
		return setup_result::error;
	auto v = v_get_stall(endpoint(s), endpoint_direction(s));
	if (v < 0)
		return setup_result::error;
	if (v)
		status |= ch9::EndpointStatus::Halt;
	memcpy(setup_buf_, &status, sizeof(status));
	txn_->set_buf(setup_buf_, sizeof(status));
	return setup_result::data;
}

/*
 * udc::endpoint_feature_request
 *
 * Caller must hold lock_.
 */
setup_result
udc::endpoint_feature_request(const setup_request &s, bool set)
{
	lock_.assert_locked();

	if (request_direction(s) != ch9::Direction::HostToDevice)
		return setup_result::error;
	if (feature(s) != ch9::FeatureSelector::ENDPOINT_HALT)
		return setup_result::error;
	if (endpoint(s) >= endpoints_)
		return setup_result::error;
	v_set_stall(endpoint(s), endpoint_direction(s), set);
	return setup_result::status;
}

/*
 * udc::process_events
 */
void
udc::process_events()
{
	std::lock_guard l{lock_};

	const auto e = events_.exchange(0);

	if (!running_)
		return;

	if (e & ep_complete_event) {
		auto c = complete_.exchange(0);
		while (c) {
			auto i = std::countr_zero(c);
			c -= 1ul << i;
			v_complete(i / 2, static_cast<ch9::Direction>(i % 2));
		}
	}

	if (e & reset_event) {
		dbg("%s: reset\n", name_.c_str());
		if (v_reset() < 0)
			state_ = ch9::DeviceState::Failed;
		else
			state_ = ch9::DeviceState::Powered;
		device_->reset();
	}

	if (e & bus_reset_event) {
		dbg("%s: bus reset\n", name_.c_str());
		if (v_bus_reset() < 0)
			state_ = ch9::DeviceState::Failed;
		else
			state_ = ch9::DeviceState::Powered;
		device_->reset();
	}

	if (e & port_change_event) {
		dbg("%s: port_change %s%s\n", name_.c_str(),
		    attached_irq_ ? "Attached" : "Detached",
		    attached_irq_
		        ?  (const char *[]){", Low Speed", ", Full Speed",
			    ", High Speed"}[static_cast<int>(speed_irq_.load())]
			: "");
		if (v_port_change() < 0)
			state_ = ch9::DeviceState::Failed;
		else if (!attached_irq_) {
			state_ = ch9::DeviceState::Detached;
			device_->reset();
		} else if (state_ != ch9::DeviceState::Default) {
			state_ = ch9::DeviceState::Default;

			v_close_endpoint(0, ch9::Direction::HostToDevice);
			v_close_endpoint(0, ch9::Direction::DeviceToHost);
			v_open_endpoint(0, ch9::Direction::HostToDevice,
			    ch9::TransferType::Control,
			    control_max_packet_len(Speed::High));
			v_open_endpoint(0, ch9::Direction::DeviceToHost,
			    ch9::TransferType::Control,
			    control_max_packet_len(Speed::High));
		}
	}

	if (e & setup_aborted_event)
		v_setup_aborted(0);

	if (e & setup_request_event)
		process_setup(setup_data_irq_.load(), speed_irq_.load());
}

/*
 * udc::th_fn
 */
void
udc::th_fn()
{
	while (semaphore_.wait_interruptible() == 0)
		process_events();
	sch_testexit();
}

/*
 * udc::th_fn_wrapper
 */
void
udc::th_fn_wrapper(void *arg)
{
	static_cast<udc *>(arg)->th_fn();
}

namespace {

/*
 * USB device controller list
 */
a::spinlock udcs_lock;
std::list<udc *> udcs;

}

/*
 * udc::add
 */
void
udc::add(udc *u)
{
	std::lock_guard l{udcs_lock};

	for (auto e : udcs) {
		if (e->name() == u->name()) {
			dbg("udc::add: %s duplicate udc\n", u->name().c_str());
			return;
		}
	}
	udcs.emplace_back(u);
}

/*
 * udc::find
 */
udc *
udc::find(std::string_view name)
{
	std::lock_guard l{udcs_lock};

	for (auto u : udcs)
		if (u->name() == name)
			return u;
	return nullptr;
}

}
