#pragma once

/*
 * Abstract USB Device
 */

#include <cstdint>
#include <dev/usb/gadget/setup_result.h>
#include <dev/usb/setup_request.h>
#include <dev/usb/string_descriptor.h>
#include <dev/usb/usb.h>
#include <span>
#include <string>
#include <vector>

namespace usb::gadget {

class transaction;

class device {
public:
	using bytes = std::span<const std::byte>;

	device(Class bDeviceClass, uint8_t bDeviceSubClass,
	    uint8_t bDeviceProtocol, uint16_t idVendor, uint16_t idProduct,
	    uint16_t bcdDevice, std::string_view Manufacturer,
	    std::string_view Product, std::string_view SerialNumber);
	virtual ~device();

	void add_string(string_descriptor &);

	/* for use by udc only */
	int init();
	void reset();
	setup_result process_setup(const setup_request &, Speed, transaction &);

	size_t max_endpoints() const;
	size_t configurations(Speed) const;
	size_t active_configuration() const;
	size_t active_interfaces() const;

private:
	virtual int v_init() = 0;
	virtual void v_reset() = 0;
	virtual setup_result v_process_setup(const setup_request &, Speed,
	    transaction &) = 0;
	virtual size_t v_max_endpoints() const = 0;
	virtual size_t v_configurations(Speed) const = 0;
	virtual size_t v_interfaces(size_t) const = 0;
	virtual bytes v_configuration_descriptors(size_t, Speed) = 0;

	setup_result device_request(const setup_request &, Speed,
	    transaction &);
	setup_result device_get_descriptor_request(const setup_request &,
	    Speed, transaction &);
	setup_result device_get_configuration_request(const setup_request &,
	    transaction &);

	ch9::device_descriptor device_;
	ch9::device_qualifier_descriptor qualifier_;
	size_t configuration_;
	string_descriptor languages_;
	string_descriptor manufacturer_;
	string_descriptor product_;
	string_descriptor serial_number_;

	union {
		uint8_t configuration;
	} setup_buf_;

	std::vector<const string_descriptor *> strings_;
};

}
