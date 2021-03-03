#pragma once

#include <dev/usb/class/cdc.h>

/*
 * Definitions from Universal Serial Bus Communications Class Subclass
 * Specification for PSTN Devices Revision 1.2
 *
 * Where possible naming & capitalisation follow the USB specification
 */

namespace usb::cdc::pstn {

/*
 * 5.3.1 Call Management Functional Descriptor
 */
struct call_management_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bmCapabilities;
	uint8_t bDataInterface;
} __attribute__((packed));

/*
 * 5.3.2 Abstract Control Management Functional Descriptor
 */
struct abstract_control_management_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bmCapabilities;
} __attribute__((packed));

/*
 * 5.3.3 Direct Line Management Functional Descriptor
 */
struct direct_line_management_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bmCapabilities;
} __attribute__((packed));

/*
 * 5.3.4 Telephone Ringer Functional Descriptor
 */
struct telephone_ringer_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bRingerVolSteps;
	uint8_t bNumRingerPatterns;
} __attribute__((packed));

/*
 * 5.3.5 Telephone Operational Modes Functional Descriptor
 */
struct telephone_operational_modes_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bmCapabilities;
} __attribute__((packed));

/*
 * 5.3.6 Telephone Call and Line State Reporting Capabilities Descriptor
 */
struct telephone_call_state_reporting_capabilities_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bmCapabilities;
} __attribute__((packed));

/*
 * Table 17: Line Coding Structure
 */
struct line_coding {
	uint32_t dwDTERate;
	uint8_t bCharFormat;
	uint8_t bParityType;
	uint8_t bDataBits;
} __attribute__((packed));

}
