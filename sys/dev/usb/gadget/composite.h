#ifndef dev_usb_gadget_composite_h
#define dev_usb_gadget_composite_h

/*
 * USB Composite Device
 */

#include "device.h"

#include <sync.h>
#include <vector>

namespace usb::gadget {

class configuration;
class function;

class composite final : public device {
public:
	composite(Class bDeviceClass, uint8_t bDeviceSubClass,
	    uint8_t bDeviceProtocol, uint16_t idVendor, uint16_t idProduct,
	    uint16_t bcdDevice, std::string_view Manufacturer,
	    std::string_view Product, std::string_view SerialNumber);

	int add_configuration(std::string_view name,
	    std::string_view description, uint8_t attributes,
	    uint8_t max_power);
	int add_function(std::string_view configs, std::shared_ptr<function>);

private:
	int v_init() override;
	void v_reset() override;
	setup_result v_process_setup(const setup_request &, Speed,
	    transaction &) override;
	size_t v_max_endpoints() const override;
	size_t v_configurations(Speed) const override;
	size_t v_interfaces(size_t) const override;
	bytes v_configuration_descriptors(size_t, Speed) override;

	setup_result device_request(const setup_request &, Speed,
	    transaction &);
	setup_result device_set_configuration_request(const setup_request &,
	    Speed, transaction &);

	configuration *find_configuration(std::string_view name);

	a::spinlock lock_;
	bool initialised_;
	std::vector<std::unique_ptr<configuration>> configurations_;
	std::vector<std::byte> desc_;
};

}

#endif
