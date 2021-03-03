#pragma once

#include <stdint.h>
#include <sys/ioctl.h>

/*
 * Set USB device descriptor
 */
struct usbg_ioctl_set_device_descriptor {
	const char *udc;	    /* name of usb device controller */
	uint8_t bDeviceClass;	    /* USB device class code */
	uint8_t bDeviceSubClass;    /* USB device subclass code */
	uint8_t bDeviceProtocol;    /* USB device protocol code */
	uint16_t idVendor;	    /* USB vendor ID */
	uint16_t idProduct;	    /* USB product ID */
	uint16_t bcdDevice;
	const char *Manufacturer;
	const char *Product;
	const char *SerialNumber;
};
#define USBG_IOC_SET_DEVICE_DESCRIPTOR _IOW('u', 0, \
				struct usbg_ioctl_set_device_descriptor)

/*
 * Add configuration to USB device
 */
struct usbg_ioctl_add_configuration {
	const char *udc;	    /* name of usb device controller */
	const char *name;	    /* name of this configuration */
	const char *Configuration;  /* USB configuration description */
	uint8_t bmAttributes;	    /* USB attributes for configuration */
	uint8_t bMaxPower;	    /* USB current consumption for config */
};
#define USBG_IOC_ADD_CONFIGURATION _IOW('u', 1, \
				struct usbg_ioctl_add_configuration)

/*
 * Add function to USB configuration(s)
 */
struct usbg_ioctl_add_function {
	const char *udc;	    /* name of usb device controller */
	const char *configs;	    /* list of configurations */
	const char *function;	    /* name of function */
	const char *data;	    /* initialisation data for function */
};
#define USBG_IOC_ADD_FUNCTION _IOW('u', 2, struct usbg_ioctl_add_function)

/*
 * Start USB device controller
 */
#define USBG_IOC_START _IOW('u', 3, const char *)

/*
 * Stop USB device controller
 */
#define USBG_IOC_STOP _IOW('u', 4, const char *)

/*
 * Get USB device controller state
 *
 * <0: Error, see errno
 *  0: Detached
 *  1: Attached
 *  2: Powered
 *  3: Default
 *  4: Address
 *  5: Configured
 *  6: Suspended
 *  7: Failed
 */
#define USBG_IOC_STATE _IOW('u', 5, const char *)
