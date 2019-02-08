#ifndef dev_usb_gadget_descriptors_h
#define dev_usb_gadget_descriptors_h

/*
 * Convenience functions for building USB descriptors
 */

#include <dev/usb/ch9.h>

namespace usb::gadget {

/*
 * Build an interface descriptor.
 */
inline constexpr ch9::interface_descriptor
interface_descriptor(const uint8_t interface_number,
    const uint8_t alternate_setting, const uint8_t num_endpoints,
    const Class interface_class, const auto interface_subclass,
    const auto interface_protocol, const uint8_t interface_string)
{
	return {
		sizeof(ch9::interface_descriptor),
		ch9::DescriptorType::INTERFACE,
		interface_number,
		alternate_setting,
		num_endpoints,
		interface_class,
		static_cast<uint8_t>(interface_subclass),
		static_cast<uint8_t>(interface_protocol),
		interface_string
	};
}

/*
 * Build an interface association descriptor.
 */
inline constexpr ch9::interface_association_descriptor
interface_association_descriptor(const uint8_t first_instance,
    const uint8_t interface_count, const Class function_class,
    const auto function_subclass, const auto function_protocol,
    const uint8_t function_string)
{
	return {
		sizeof(ch9::interface_association_descriptor),
		ch9::DescriptorType::INTERFACE_ASSOCIATION,
		first_instance,
		interface_count,
		function_class,
		static_cast<uint8_t>(function_subclass),
		static_cast<uint8_t>(function_protocol),
		function_string
	};
}

/*
 * Build a bulk endpoint descriptor.
 */
inline constexpr ch9::endpoint_descriptor
bulk_endpoint_descriptor(const size_t endpoint, const ch9::Direction dir,
    const uint16_t max_packet)
{
	return {
		sizeof(ch9::endpoint_descriptor),
		ch9::DescriptorType::ENDPOINT,
		static_cast<uint8_t>(endpoint | static_cast<int>(dir) << 7),
		static_cast<uint8_t>(ch9::TransferType::Bulk),
		htole16(max_packet),
		0
	};
}

/*
 * Build an interrupt endpoint descriptor.
 */
inline constexpr ch9::endpoint_descriptor
interrupt_endpoint_descriptor(const size_t endpoint, const ch9::Direction dir,
    const uint16_t max_packet, const uint8_t interval)
{
	return {
		sizeof(ch9::endpoint_descriptor),
		ch9::DescriptorType::ENDPOINT,
		static_cast<uint8_t>(endpoint | static_cast<int>(dir) << 7),
		static_cast<uint8_t>(ch9::TransferType::Interrupt),
		htole16(max_packet),
		interval
	};
}

};

#endif
