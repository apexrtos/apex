#ifndef dev_usb_setup_request_h
#define dev_usb_setup_request_h

/*
 * USB setup request
 */

#include "ch9.h"
#include <cassert>
#include <cstddef>
#include <endian.h>

namespace usb {

class setup_request final {
public:
	setup_request(const ch9::setup_data &s)
	: s_{s}
	{ }

	size_t request_type() const
	{
		return s_.bmRequestType;
	}

	size_t request() const
	{
		return s_.bRequest;
	}

	size_t value() const
	{
		return le16toh(s_.wValue);
	}

	size_t index() const
	{
		return le16toh(s_.wIndex);
	}

	size_t length() const
	{
		return le16toh(s_.wLength);
	}

protected:
	ch9::setup_data s_;	/* stored in USB (little endian) order */
};

static inline ch9::Direction
request_direction(const setup_request &s)
{
	return static_cast<ch9::Direction>(s.request_type() >> 7);
}

static inline ch9::RequestType
request_type(const setup_request &s)
{
	return static_cast<ch9::RequestType>((s.request_type() >> 5) & 0x3);
}

static inline ch9::RequestRecipient
request_recipient(const setup_request &s)
{
	return static_cast<ch9::RequestRecipient>(s.request_type() & 0x1f);
}

static inline ch9::Request
standard_request(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	return static_cast<ch9::Request>(s.request());
}

static inline ch9::DescriptorType
descriptor_type(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	assert(standard_request(s) == ch9::Request::GET_DESCRIPTOR);
	return static_cast<ch9::DescriptorType>(s.value() >> 8);
}

static inline size_t
descriptor_index(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	assert(standard_request(s) == ch9::Request::GET_DESCRIPTOR);
	return s.value() & 0xff;
}

static inline size_t
language_id(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	assert(standard_request(s) == ch9::Request::GET_DESCRIPTOR);
	return s.index();
}

static inline size_t
interface(const setup_request &s)
{
	assert(request_recipient(s) == ch9::RequestRecipient::Interface);
	return s.index();
}

static inline size_t
endpoint(const setup_request &s)
{
	assert(request_recipient(s) == ch9::RequestRecipient::Endpoint);
	return s.index() & 0x7f;
}

static inline ch9::Direction
endpoint_direction(const setup_request &s)
{
	assert(request_recipient(s) == ch9::RequestRecipient::Endpoint);
	return s.index() & 0x80
	    ? ch9::Direction::DeviceToHost
	    : ch9::Direction::HostToDevice;
}

static inline ch9::FeatureSelector
feature(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	assert(standard_request(s) == ch9::Request::CLEAR_FEATURE ||
	    standard_request(s) == ch9::Request::SET_FEATURE);
	return static_cast<ch9::FeatureSelector>(s.value());
}

static inline size_t
address(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	assert(standard_request(s) == ch9::Request::SET_ADDRESS);
	return s.value();
}

static inline size_t
configuration(const setup_request &s)
{
	assert(request_type(s) == ch9::RequestType::Standard);
	assert(standard_request(s) == ch9::Request::SET_CONFIGURATION);
	return s.value();
}

}

#endif
