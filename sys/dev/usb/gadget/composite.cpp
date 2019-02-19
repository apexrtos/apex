#include "composite.h"

#include "configuration.h"
#include <cerrno>
#include <debug.h>
#include <mutex>
#include <string_utils.h>

namespace usb::gadget {

/*
 * composite::composite
 */
composite::composite(Class bDeviceClass, uint8_t bDeviceSubClass,
    uint8_t bDeviceProtocol, uint16_t idVendor, uint16_t idProduct,
    uint16_t bcdDevice, std::string_view Manufacturer,
    std::string_view Product, std::string_view SerialNumber)
: device{bDeviceClass, bDeviceSubClass, bDeviceProtocol, idVendor, idProduct,
    bcdDevice, Manufacturer, Product, SerialNumber}
, initialised_{false}
{ }

/*
 * composite::add_configuration - add a configuration to composite device
 */
int
composite::add_configuration(std::string_view name,
    std::string_view description, uint8_t attributes, uint8_t max_power)
{
	std::lock_guard l{lock_};

	if (initialised_)
		return -EBUSY;

	configurations_.emplace_back(std::make_unique<configuration>(name,
	    description, attributes, max_power));

	return 0;
}

/*
 * composite::add_functin - add a function to composite device
 */
int
composite::add_function(std::string_view configs, std::shared_ptr<function> f)
{
	std::lock_guard l{lock_};

	if (initialised_)
		return -EBUSY;

	int ret{0};

	/* make sure all configurations exist */
	strtok(configs, ", \t\n", [&](auto config) {
		auto c{find_configuration(config)};
		if (!c)
			ret = -ENODEV;
	});
	if (ret)
		return ret;

	/* add function to configurations */
	strtok(configs, ", \t\n", [&](auto config) {
		find_configuration(config)->add_function(f);
	});

	return 0;
}

/*
 * composite::v_init - initialise composite device
 */
int
composite::v_init()
{
	std::lock_guard l{lock_};

	size_t sz{0};

	for (size_t i = 0; i < configurations_.size(); ++i) {
		auto &c = configurations_[i];
		if (auto r = c->init(*this, i + 1); r < 0)
			return r;
		sz = std::max(sz, c->sizeof_descriptors(Speed::Low));
		sz = std::max(sz, c->sizeof_descriptors(Speed::High));
		sz = std::max(sz, c->sizeof_descriptors(Speed::Full));
	}

	desc_.resize(sz);

	initialised_ = true;

	return 0;
}

/*
 * composite::v_finalise - finalise composite device
 */
int
composite::v_finalise()
{
	std::lock_guard l{lock_};

	for (auto &c : configurations_)
		if (int err = c->finalise(); err)
			return err;

	return 0;
}

/*
 * composite::v_reset - reset composite device
 */
void
composite::v_reset()
{
	std::lock_guard l{lock_};

	for (auto &c : configurations_)
		c->reset();

	initialised_ = false;
}

/*
 * composite::v_process_setup - attempt to process setup request
 */
setup_result
composite::v_process_setup(const setup_request &s, const Speed spd,
    transaction &t)
{
	/* try configuration first */
	if (active_configuration()) {
		auto &c = configurations_[active_configuration() - 1];
		if (auto r = c->process_setup(s, t); r != setup_result::error)
			return r;
	}

	/* handle some standard requests here */
	if (request_type(s) != ch9::RequestType::Standard)
		return setup_result::error;
	switch (request_recipient(s)) {
	case ch9::RequestRecipient::Device:
		return device_request(s, spd, t);
	default:
		return setup_result::error;
	}
}

/*
 * composite::v_max_endpoints - return maximum endpoints required by any
 *				configuration
 */
size_t
composite::v_max_endpoints() const
{
	size_t ep = 0;
	for (auto &c : configurations_)
		if (auto v = c->endpoints(); v > ep)
			ep = v;
	return ep;
}

/*
 * composite::v_configurations - return number of configurations
 */
size_t
composite::v_configurations(const Speed spd) const
{
	return configurations_.size();
}

/*
 * composite::v_interfaces - return number of interfaces for a configuration
 */
size_t
composite::v_interfaces(size_t config) const
{
	assert(config > 0);
	assert(config <= configurations_.size());
	return configurations_[config - 1]->interfaces();
}

/*
 * composite::v_configuration_descriptors - return configuration descriptors
 *					    for a configuration running at a
 *					    specified speed
 */
composite::bytes
composite::v_configuration_descriptors(size_t idx, const Speed spd)
{
	assert(idx < configurations_.size());
	auto len = configurations_[idx]->write_descriptors(spd, desc_);
	return {data(desc_), static_cast<ptrdiff_t>(len)};
}

/*
 * composite::device_request - attempt to handle a device request
 */
setup_result
composite::device_request(const setup_request &s, const Speed spd,
    transaction &t)
{
	switch (standard_request(s)) {
	case ch9::Request::SET_CONFIGURATION:
		return device_set_configuration_request(s, spd, t);
	default:
		return setup_result::error;
	};
}

/*
 * composite::device_set_configuration_request - attempt to handle a device
 *						 set configuraiton request
 */
setup_result
composite::device_set_configuration_request(const setup_request &s,
    const Speed spd, transaction &t)
{
	if (request_direction(s) != ch9::Direction::HostToDevice)
		return setup_result::error;

	const auto c = usb::configuration(s);

	/* setting configuration 0 returns device to address state */
	if (!c) {
		if (active_configuration())
			configurations_[active_configuration() - 1]->stop();
		return setup_result::status;
	}
	if (c > configurations_.size()) {
		dbg("composite::set_configuration_request invalid %d\n", c);
		return setup_result::error;
	}
	if (auto r = configurations_[c - 1]->start(spd); r < 0)
		return setup_result::error;

	return setup_result::status;
}

/*
 * composite::find_configuration - find a configuration of a particular name
 */
configuration *
composite::find_configuration(std::string_view name)
{
	for (auto &c : configurations_)
		if (c->name() == name)
			return c.get();
	return nullptr;
}

}
