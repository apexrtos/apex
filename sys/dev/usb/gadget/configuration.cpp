#include "configuration.h"

#include "device.h"
#include "function.h"
#include <debug.h>
#include <endian.h>
#include <list>

namespace usb::gadget {

/*
 * configuration::configuration
 */
configuration::configuration(std::string_view name,
    std::string_view description, uint8_t attributes, uint8_t max_power)
: name_{name}
, description_{description}
, conf_{}
{
	conf_.bLength = sizeof(conf_);
	conf_.bDescriptorType = ch9::DescriptorType::CONFIGURATION;
	conf_.bmAttributes = attributes;
	conf_.bMaxPower = max_power;
}

/*
 * configuration::init - initialise configuration
 */
int
configuration::init(device &d, const size_t index)
{
	d.add_string(description_);

	/* Initalise functions, count interfaces and assign endpoints starting
	 * from 1 (ep 0 is default control pipe). */
	size_t numep = 1;
	size_t numif = 0;
	for (const auto &f : functions_) {
		if (auto r = f->init(d); r < 0)
			return r;
		f->set_offsets(numep, numif);
		numif += f->interfaces();
		numep += f->endpoints();
	}

	conf_.bNumInterfaces = numif;
	conf_.bConfigurationValue = index;

	return 0;
}

/*
 * configuration::reset - reset configuration
 */
void
configuration::reset()
{
	for (auto &f : functions_)
		f->reset();
}

/*
 * configuration::start - start configuration
 */
int
configuration::start(const Speed spd)
{
	for (auto &f : functions_)
		if (auto r = f->start(spd); r < 0)
			return r;
	return 0;
}

/*
 * configuration::stop - stop configuration
 */
void
configuration::stop()
{
	for (auto &f : functions_)
		f->stop();
}

/*
 * configuration::process_setup - attempt to process setup request
 */
setup_result
configuration::process_setup(const setup_request &s, transaction &t)
{
	switch (request_recipient(s)) {
	case ch9::RequestRecipient::Interface:
		return interface_request(s, t);
	default:
		return setup_result::error;
	}
}

/*
 * configuration::add_function - add function to configuration
 */
void
configuration::add_function(std::shared_ptr<function> f)
{
	functions_.emplace_back(std::move(f));
}

/*
 * configuration::name - return configuration name
 */
const std::string &
configuration::name() const
{
	return name_;
}

/*
 * configuration::description - return configuration description
 */
const string_descriptor &
configuration::description() const
{
	return description_;
}

/*
 * configuration::endpoints - return number of endpoints for configuration
 */
size_t
configuration::endpoints() const
{
	size_t ep = 0;
	for (auto &f : functions_)
		ep += f->endpoints();
	return ep;
}

/*
 * configuration::interfaces - return number of interfaces for configuration
 */
size_t
configuration::interfaces() const
{
	size_t i = 0;
	for (auto &f : functions_)
		i += f->interfaces();
	return i;
}

/*
 * configuration::sizeof_descriptors - return size of descriptors for this
 *				       configuration running at specified speed
 */
size_t
configuration::sizeof_descriptors(const Speed spd) const
{
	size_t sz = sizeof(ch9::configuration_descriptor);
	for (const auto &f : functions_)
		sz += f->sizeof_descriptors(spd);
	return sz;
}

/*
 * configuration::write_descriptors - write descriptors for this configuration
 *				      running at specified speed
 */
size_t
configuration::write_descriptors(const Speed spd, std::span<std::byte> m)
{
	auto pos = begin(m);

	/* configuration descriptor */
	conf_.wTotalLength = htole16(sizeof_descriptors(spd));
	conf_.iConfiguration = description_.index();

	const auto len = std::min<size_t>(sizeof(conf_), m.size());
	memcpy(&pos[0], &conf_, len);
	pos += len;

	/* function descriptors */
	for (auto &f : functions_)
		pos += f->write_descriptors(spd, {&pos[0],
					    static_cast<size_t>(end(m) - pos)});

	return pos - begin(m);
}

/*
 * configuration::interface_request - attempt to handle interface request
 */
setup_result
configuration::interface_request(const setup_request &s, transaction &t)
{
	/* dispatch request to function containing interface */
	for (auto &f : functions_)
		if (interface(s) >= f->interface_offset() &&
		    interface(s) < f->interface_offset() + f->interfaces())
			return f->process_setup(s, t);
	return setup_result::error;
}

}
