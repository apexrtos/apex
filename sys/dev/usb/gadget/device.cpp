#include "device.h"

#include <debug.h>
#include <dev/usb/gadget/transaction.h>
#include <endian.h>

#define trace(...)

namespace {

/*
 * other - return 'other' speed
 */
constexpr usb::Speed
other(const usb::Speed spd)
{
	switch (spd) {
	case usb::Speed::Low:
	case usb::Speed::Full:
		return usb::Speed::High;
	case usb::Speed::High:
		return usb::Speed::Full;
	}
	__builtin_unreachable();
}

}

namespace usb::gadget {

/*
 * device::device
 */
device::device(Class bDeviceClass, uint8_t bDeviceSubClass,
    uint8_t bDeviceProtocol, uint16_t idVendor, uint16_t idProduct,
    uint16_t bcdDevice, std::string_view Manufacturer,
    std::string_view Product, std::string_view SerialNumber)
: device_{
    .bLength = sizeof(device_),
    .bDescriptorType = ch9::DescriptorType::DEVICE,
    .bcdUSB = htole16(0x200),
    .bDeviceClass = bDeviceClass,
    .bDeviceSubClass = bDeviceSubClass,
    .bDeviceProtocol = bDeviceProtocol,
    .bMaxPacketSize0 = 0,
    .idVendor = htole16(idVendor),
    .idProduct = htole16(idProduct),
    .bcdDevice = htole16(bcdDevice),
    .iManufacturer = 0,
    .iProduct = 0,
    .iSerialNumber = 0,
    .bNumConfigurations = 0}
, qualifier_{
    .bLength = sizeof(qualifier_),
    .bDescriptorType = ch9::DescriptorType::DEVICE_QUALIFIER,
    .bcdUSB = htole16(0x200),
    .bDeviceClass = bDeviceClass,
    .bDeviceSubClass = bDeviceSubClass,
    .bDeviceProtocol = bDeviceProtocol,
    .bMaxPacketSize0 = 0,
    .bNumConfigurations = 0,
    .bReserved = 0}
, configuration_{0}
, languages_{u"\x0904"}	/* US english */
, manufacturer_{Manufacturer}
, product_{Product}
, serial_number_{SerialNumber}
{ }

/*
 * device::~device
 */
device::~device()
{ }

/*
 * device::add_string - add a string to the string table
 */
void
device::add_string(string_descriptor &s)
{
	/* deduplicate strings */
	if (auto f = find_if(begin(strings_), end(strings_),
	    [&s](const auto &v) { return *v == s; }); f != end(strings_)) {
		s.set_index(f - begin(strings_));
		return;
	}
	strings_.push_back(&s);
	s.set_index(strings_.size() - 1);
}

/*
 * device::init - initialise device
 */
int
device::init()
{
	strings_.clear();
	add_string(languages_);
	add_string(manufacturer_);
	add_string(product_);
	add_string(serial_number_);

	device_.iManufacturer = manufacturer_.index();
	device_.iProduct = product_.index();
	device_.iSerialNumber = serial_number_.index();
	return v_init();
}

/*
 * device::reset - reset device
 */
void
device::reset()
{
	v_reset();
	configuration_ = 0;
}

/*
 * device::process_setup - attempt to process a setup request
 */
setup_result
device::process_setup(const setup_request &s, const Speed spd, transaction &t)
{
	/* try device implementation first */
	if (auto r = v_process_setup(s, spd, t); r != setup_result::error) {
		/* track configuration */
		if (request_type(s) == ch9::RequestType::Standard &&
		    request_direction(s) == ch9::Direction::HostToDevice &&
		    request_recipient(s) == ch9::RequestRecipient::Device &&
		    standard_request(s) == ch9::Request::SET_CONFIGURATION)
			configuration_ = configuration(s);
		return r;
	}

	/* handle some standard requests here */
	if (request_type(s) != ch9::RequestType::Standard)
		return setup_result::error;
	if (request_recipient(s) != ch9::RequestRecipient::Device)
		return setup_result::error;
	return device_request(s, spd, t);
}

/*
 * device::max_endpoints - return maximum number of endpoints for device
 */
size_t
device::max_endpoints() const
{
	return v_max_endpoints();
}

/*
 * device::configurations - return number of configurations for device
 */
size_t
device::configurations(const Speed spd) const
{
	return v_configurations(spd);
}

/*
 * device::active_configuration - return current active configuration
 */
size_t
device::active_configuration() const
{
	return configuration_;
}

/*
 * device::active_interfaces - return current number of active interfaces
 */
size_t
device::active_interfaces() const
{
	if (!configuration_)
		return 0;
	return v_interfaces(configuration_);
}

/*
 * device::device_request - attempt to handle a device request
 */
setup_result
device::device_request(const setup_request &s, const Speed spd, transaction &t)
{
	switch (standard_request(s)) {
	case ch9::Request::GET_DESCRIPTOR:
		return device_get_descriptor_request(s, spd, t);
	case ch9::Request::GET_CONFIGURATION:
		return device_get_configuration_request(s, t);
	default:
		return setup_result::error;
	};
}

/*
 * device::device_get_descriptor_request - attempt to handle a get descriptor
 *					   request on this device
 */
setup_result
device::device_get_descriptor_request(const setup_request &s, const Speed spd,
    transaction &t)
{
	if (request_direction(s) != ch9::Direction::DeviceToHost)
		return setup_result::error;

	trace("device::get_descriptor: type %d index %zu lang %zu len %zu\n",
	    static_cast<int>(descriptor_type(s)), descriptor_index(s),
	    language_id(s), s.length());

	t.set_zero_length_termination(true);

	switch (descriptor_type(s)) {
	case ch9::DescriptorType::DEVICE:
		trace("device::get_descriptor: DEVICE\n");
		device_.bMaxPacketSize0 = control_max_packet_len(spd);
		device_.bNumConfigurations = configurations(spd);
		t.set_buf(&device_, std::min(s.length(), sizeof(device_)));
		return setup_result::data;
	case ch9::DescriptorType::CONFIGURATION: {
		/* configuration descriptor reads are special, they get
		 * the configuration, interface, endpoint and class- or
		 * vendor-specific descriptors in one transaction */
		trace("device::get_descriptor: CONFIGURATION\n");
		if (descriptor_index(s) >= configurations(spd)) {
			dbg("device::get_descriptor CONFIGURATION %d invalid\n",
			    descriptor_index(s));
			return setup_result::error;
		}
		const auto &d = v_configuration_descriptors(
		    descriptor_index(s), spd);
		t.set_buf(data(d), std::min<size_t>(s.length(), size(d)));
		return setup_result::data;
	}
	case ch9::DescriptorType::STRING: {
		/* string descriptor 0 is special, it's the language table */
		trace("device::get_descriptor: STRING\n");
		if (descriptor_index(s) >= strings_.size()) {
			dbg("device::get_descriptor STRING %d invalid\n",
			    descriptor_index(s));
			return setup_result::error;
		}
		const auto &d = strings_[descriptor_index(s)]->data();
		t.set_buf(data(d), std::min(s.length(), size(d)));
		return setup_result::data;
	}
	case ch9::DescriptorType::DEVICE_QUALIFIER:
		trace("device::get_descriptor: DEVICE_QUALIFIER\n");
		qualifier_.bMaxPacketSize0 = control_max_packet_len(other(spd));
		qualifier_.bNumConfigurations = configurations(other(spd));
		t.set_buf(&qualifier_, std::min(s.length(), sizeof(qualifier_)));
		return setup_result::data;
	case ch9::DescriptorType::OTHER_SPEED_CONFIGURATION: {
		trace("device::get_descriptor: OTHER_SPEED_CONFIGURATION\n");
		if (descriptor_index(s) >= configurations(other(spd))) {
			dbg("device::get_descriptor OTHER_SPEED_CONFIGURATION "
			    "%d invalid\n", descriptor_index(s));
			return setup_result::error;
		}
		const auto &d = v_configuration_descriptors(
		    descriptor_index(s), other(spd));
		t.set_buf(data(d), std::min<size_t>(s.length(), size(d)));
		return setup_result::data;
	}
	default:
		dbg("device::get_descriptor descriptor %d not supported\n",
		    static_cast<int>(descriptor_type(s)));
		return setup_result::error;
	}
}

/*
 * device::device_get_descriptor_request - attempt to handle a get
 *					   configuration request on this device
 */
setup_result
device::device_get_configuration_request(const setup_request &s, transaction &t)
{
	if (request_direction(s) != ch9::Direction::DeviceToHost)
		return setup_result::error;
	if (s.length() != sizeof(setup_buf_.configuration))
		return setup_result::error;
	setup_buf_.configuration = configuration_;
	t.set_buf(&setup_buf_, sizeof(setup_buf_.configuration));
	return setup_result::data;
}

}
