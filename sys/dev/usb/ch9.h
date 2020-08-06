#ifndef dev_usb_ch9_h
#define dev_usb_ch9_h

/*
 * Definitions from Universal Serial Bus Specification Revision 2.0 Chapter 9:
 * USB Device Framework
 *
 * Where possible naming & capitalisation follow the USB specification.
 */

#include "class/class.h"
#include "usb.h"
#include <cstdint>

namespace usb::ch9 {

/*
 * 9.1: USB Device States
 */
enum class DeviceState {
	Detached,	    /* Apex: device is not attached to USB bus */
	Attached,
	Powered,
	Default,
	Address,
	Configured,
	Suspended,
	Failed,		    /* Apex: hardware failure or software bug */
};

/*
 * 9.3: USB Device Requests
 */
enum class Direction {
	HostToDevice = 0,
	DeviceToHost = 1,
};

enum class RequestType {
	Standard = 0,
	Class = 1,
	Vendor = 2,
};

enum class RequestRecipient {
	Device = 0,
	Interface = 1,
	Endpoint = 2,
	Other = 3,
};

struct setup_data {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
};
static_assert(sizeof(setup_data) == setup_packet_len, "");

/*
 * 9.4 Standard Device Requests
 */
enum class Request {
	GET_STATUS = 0,
	CLEAR_FEATURE = 1,
	SET_FEATURE = 3,
	SET_ADDRESS = 5,
	GET_DESCRIPTOR = 6,
	SET_DESCRIPTOR = 7,
	GET_CONFIGURATION = 8,
	SET_CONFIGURATION = 9,
	GET_INTERFACE = 10,
	SET_INTERFACE = 11,
	SYNCH_FRAME = 12,
};

enum class DescriptorType : uint8_t {
	/* Standard descriptors */
	DEVICE = 1,
	CONFIGURATION = 2,
	STRING = 3,
	INTERFACE = 4,
	ENDPOINT = 5,
	DEVICE_QUALIFIER = 6,
	OTHER_SPEED_CONFIGURATION = 7,
	INTERFACE_POWER = 8,
	OTG = 9,
	DEBUG = 10,
	INTERFACE_ASSOCIATION = 11,

	/* Class specific descriptors */
	CS_DEVICE = 34,
	CS_CONFIGURATION = 35,
	CS_INTERFACE = 36,
	CS_ENDPOINT = 37,
};

enum class FeatureSelector {
	ENDPOINT_HALT = 0,
	DEVICE_REMOTE_WAKEUP = 1,
	TEST_MODE = 2,
};

namespace DeviceStatus {
	inline constexpr auto SelfPowered = 0x1;
	inline constexpr auto RemoteWakeup = 0x2;
}

namespace EndpointStatus {
	inline constexpr auto Halt = 0x1;
}

/*
 * 9.5: Descriptors
 */
struct device_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint16_t bcdUSB;
	Class bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} __attribute__((packed));
static_assert(sizeof(device_descriptor) == 18, "");

struct device_qualifier_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint16_t bcdUSB;
	Class bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint8_t bNumConfigurations;
	uint8_t bReserved;
} __attribute__((packed));
static_assert(sizeof(device_qualifier_descriptor) == 10, "");

struct configuration_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint16_t wTotalLength;
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__((packed));
static_assert(sizeof(configuration_descriptor) == 9, "");

struct interface_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	Class bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t iInterface;
} __attribute__((packed));
static_assert(sizeof(interface_descriptor) == 9, "");

enum class TransferType {
	Control = 0,
	Isochronous = 1,
	Bulk = 2,
	Interrupt = 3,
};

enum class IsochronousSynchronizationType {
	No_Synchronization = 0 << 2,
	Asynchronous = 1 << 2,
	Adaptive = 2 << 2,
	Synchronous = 3 << 2,
};

enum class IsochronousUsageType {
	Data = 0 << 4,
	Feedback = 1 << 4,
	Implicit_Feedback = 2 << 4,
};

struct endpoint_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint8_t bEndpointAddress;
	uint8_t bmAttributes;
	uint16_t wMaxPacketSize;
	uint8_t bInterval;
} __attribute__((packed));
static_assert(sizeof(endpoint_descriptor) == 7, "");

struct string_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	char bString[];
} __attribute__((packed));
static_assert(sizeof(string_descriptor) == 2, "");

struct interface_association_descriptor {
	uint8_t bLength;
	DescriptorType bDescriptorType;
	uint8_t bFirstInstance;
	uint8_t bInterfaceCount;
	Class bFunctionClass;
	uint8_t bFunctionSubClass;
	uint8_t bFunctionProtocol;
	uint8_t iFunction;
} __attribute__((packed));
static_assert(sizeof(interface_association_descriptor) == 8, "");

}

#endif
