#ifndef dev_usb_class_cdc_h
#define dev_usb_class_cdc_h

#include <dev/usb/ch9.h>

/*
 * Definitions from Universal Serial Bus Class Definitions for Communications
 * Devices Revision 1.2 (Errata 1)
 *
 * Where possible naming & capitalisation follow the USB specification.
 */

namespace usb::cdc {

constexpr auto version = 0x0120;

/*
 * 4.3 Communications Class Subclass Codes
 */
enum class SubClass {
	Direct_Line_Control_Model = 0x1,
	Abstract_Control_Model = 0x2,
	Telephone_Control_Model = 0x3,
	MultiChannel_Control_Model = 0x4,
	CAPI_Control_Model = 0x5,
	Ethernet_Networking_Control_Model = 0x6,
	ATM_Networking_Control_Model = 0x7,
	Wireless_Handset_Control_Model = 0x8,
	Device_Management = 0x9,
	Mobile_Direct_Line_Model = 0xa,
	OBEX = 0xb,
	Ethernet_Emulation_Model = 0xc,
	Network_Control_Model = 0xd,
};

/*
 * 4.4 Communications Class Protocol Codes
 */
enum class Protocol {
	Not_Required = 0x0,
	AT_V_250 = 0x1,
	AT_PCCA_101 = 0x2,
	AT_PCCA_101_Annex_O = 0x3,
	AT_GSM_07_07 = 0x4,
	AT_3GPP_27_007 = 0x5,
	AT_TIA_CDMA = 0x6,
	Ethernet_Emulation_Model = 0x7,
	External = 0xfe,
	Vendor_Specific = 0xff,
};

/*
 * 4.7 Data Interface Class Protocol Codes
 */
enum class DataProtocol {
	Not_Required = 0x0,
	Network_Transfer_Block = 0x1,
	ISDN_BRI = 0x30,
	HDLC = 0x31,
	Q_921_M = 0x50,
	Q_921 = 0x51,
	Q_921_TM = 0x52,
	V_42bis = 0x90,
	Q_931 = 0x91,
	V_120 = 0x92,
	CAPI_2_0 = 0x93,
	Host_Based = 0xfd,
	Protocol_Unit_Defined = 0xfe,
	Vendor_Specific = 0xff,
};

/*
 * Table 13: bDescriptorSubType in Communications Class Functional Descriptors
 */
enum class Function : uint8_t {
	Header = 0x0,
	Call_Management = 0x1,
	Abstract_Control_Management = 0x2,
	Direct_Line_Management = 0x3,
	Telephone_Ringer = 0x4,
	Telephone_Call_and_Line_State_Reporting_Capabilities = 0x5,
	Union = 0x6,
	Country_Selection = 0x7,
	Telephone_Operational_Modes = 0x8,
	USB_Terminal = 0x9,
	Network_Channel = 0xa,
	Protocol_Unit = 0xb,
	Extension_Unit = 0xc,
	MultiChannel_Management = 0xd,
	CAPI_Control_Management = 0xe,
	Ethernet_Networking = 0xf,
	ATM_Networking = 0x10,
	Wireless_Handeset_Control_Model = 0x11,
	Mobile_Direct_Line_Model = 0x12,
	MDLM_Detail = 0x13,
	Device_Management_Model = 0x14,
	OBEX = 0x15,
	Command_Set = 0x16,
	Command_Set_Detail = 0x17,
	Telephone_Control_Model = 0x18,
	OBEX_Service_Identifier = 0x19,
	NCM = 0x1a,
};

/*
 * Table 19: Class-Specific Request Codes
 */
enum class Request {
	SEND_ENCAPSULATED_COMMAND = 0x0,
	GET_ENCAPSULATED_RESPONSE = 0x1,
	SET_COMM_FEATURE = 0x2,
	GET_COMM_FEATURE = 0x3,
	CLEAR_COMM_FEATURE = 0x4,
	SET_AUX_LINE_STATE = 0x10,
	SET_HOOK_STATE = 0x11,
	PULSE_SETUP = 0x12,
	SEND_PULSE = 0x13,
	SET_PULSE_TIME = 0x14,
	RING_AUX_JACK = 0x15,
	SET_LINE_CODING = 0x20,
	GET_LINE_CODING = 0x21,
	SET_CONTROL_LINE_STATE = 0x22,
	SEND_BREAK = 0x23,
	SET_RINGER_PARMS = 0x30,
	GET_RINGER_PARMS = 0x31,
	SET_OPERATION_PARMS = 0x32,
	GET_OPERATION_PARMS = 0x33,
	SET_LINE_PARMS = 0x34,
	GET_LINE_PARMS = 0x35,
	DIAL_DIGITS = 0x36,
	SET_UNIT_PARAMETER = 0x37,
	GET_UNIT_PARAMETER = 0x38,
	CLEAR_UNIT_PARAMETER = 0x39,
	GET_PROFILE = 0x3a,
	SET_ETHERNET_MULTICAST_FILTERS = 0x40,
	SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x41,
	GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER = 0x42,
	SET_ETHERNET_PACKET_FILTER = 0x43,
	GET_ETHERNET_STATISTIC = 0x44,
	SET_ATM_DATA_FORMAT = 0x50,
	GET_ATM_DEVICE_STATISTICS = 0x51,
	SET_ATM_DEFAULT_VC = 0x52,
	GET_ATM_VC_STATISTICS = 0x53,
	GET_NTB_PARAMETERS = 0x80,
	GET_NET_ADDRESS = 0x81,
	SET_NET_ADDRESS = 0x82,
	GET_NTB_FORMAT = 0x83,
	SET_NTB_FORMAT = 0x84,
	GET_NTB_INPUT_SIZE = 0x85,
	SET_NTB_INPUT_SIZE = 0x86,
	GET_MAX_DATAGRAM_SIZE = 0x87,
	SET_MAX_DATAGRAM_SIZE = 0x88,
	GET_CRC_MODE = 0x89,
	SET_CRC_MODE = 0x8a,
};

/*
 * Table 20: Class-Specific Notification Codes
 */
enum class Notification {
	NETWORK_CONNECTION = 0x0,
	RESPONSE_AVAILABLE = 0x1,
	AUX_JACK_HOOK_STATE = 0x8,
	RING_DETECT = 0x9,
	SERIAL_STATE = 0x20,
	CALL_STATE_CHANGE = 0x28,
	LINE_STATE_CHANGE = 0x29,
	CONNECTION_SPEED_CHANGE = 0x2a,
};

/*
 * 5.2.3.1: Header Functional Descriptor
 */
struct header_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint16_t bcdCDC;
} __attribute__((packed));

/*
 * 5.2.3.2: Union Functional Descriptor
 */
struct union_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t bControlInterface;
	uint8_t bSubordinateInterface[];
} __attribute__((packed));

/*
 * 5.2.3.3 Country Selection Functional Descriptor
 */
struct country_selection_functional_descriptor {
	uint8_t bFunctionLength;
	ch9::DescriptorType bDescriptorType;
	Function bDescriptorSubtype;
	uint8_t iCountryCodeRelDate;
	uint16_t wCountryCode[];
} __attribute__((packed));

}

#endif
