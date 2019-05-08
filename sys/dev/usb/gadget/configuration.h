#ifndef dev_usb_gadget_configuration_h
#define dev_usb_gadget_configuration_h

/*
 * USB Composite Device Configuration
 */

#include <dev/usb/ch9.h>
#include <dev/usb/gadget/setup_result.h>
#include <dev/usb/string_descriptor.h>
#include <list>
#include <span>
#include <string>

namespace usb { class setup_request; }

namespace usb::gadget {

class device;
class function;
class transaction;

class configuration final {
public:
	configuration(std::string_view name, std::string_view description,
	    uint8_t attributes, uint8_t max_power);

	int init(device &, size_t index);
	void reset();
	int start(Speed);
	void stop();
	setup_result process_setup(const setup_request &, transaction &);
	void add_function(std::shared_ptr<function>);

	const std::string &name() const;
	const string_descriptor &description() const;
	size_t endpoints() const;
	size_t interfaces() const;
	size_t sizeof_descriptors(Speed) const;
	size_t write_descriptors(Speed, std::span<std::byte>);

private:
	setup_result interface_request(const setup_request &, transaction &);

	const std::string name_;
	string_descriptor description_;
	ch9::configuration_descriptor conf_;

	std::list<std::shared_ptr<function>> functions_;
};

}

#endif
