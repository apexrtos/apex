#pragma once

#include <cstdint>

namespace usb {

/*
 * USB Device Classes
 */
enum class Class : uint8_t {
	Unspecified = 0x00,
	Audio = 0x01,
	CDC = 0x02,
	HID = 0x03,
	PID = 0x05,
	Image = 0x06,
	Printer = 0x07,
	Mass_Storage = 0x08,
	Hub = 0x09,
	CDC_Data = 0x0a,
	Smart_Card = 0x0b,
	Content_Security = 0x0d,
	Video = 0x0e,
	PHDC = 0x0f,
	AV = 0x10,
	Billboard = 0x11,
	Diagnostic = 0xdc,
	Wireless = 0xe0,
	Miscellaneous = 0xef,
	Application_Specific = 0xfe,
	Vendor_Specific = 0xff,
};

}
