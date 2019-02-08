#include "init.h"

#include "ioctl.h"
#include <access.h>
#include <cstring>
#include <debug.h>
#include <dev/usb/gadget/composite.h>
#include <dev/usb/gadget/configuration.h>
#include <dev/usb/gadget/function.h>
#include <dev/usb/gadget/udc.h>
#include <device.h>
#include <errno.h>
#include <fs/file.h>
#include <memory>
#include <mutex>

namespace {

using namespace usb;
inline constexpr auto string_max = 100;

/*
 * gadget_ioctl - control usb gadget framework
 *
 * See ioctl.h for more information about the usb gadget ioctl interface.
 */
int
gadget_ioctl(file *file, unsigned long cmd, void *data)
{
	switch (cmd) {
	case USBG_IOC_SET_DEVICE_DESCRIPTOR: {
		usbg_ioctl_set_device_descriptor d;
		memcpy(&d, data, sizeof(d));
		if (!u_strcheck(d.udc, string_max) ||
		    !u_strcheck(d.Manufacturer, string_max) ||
		    !u_strcheck(d.Product, string_max) ||
		    !u_strcheck(d.SerialNumber, string_max))
			return DERR(-EFAULT);
		auto u{gadget::udc::find(d.udc)};
		if (!u)
			return DERR(-ENODEV);
		u->set_device<gadget::composite>(
		    static_cast<usb::Class>(d.bDeviceClass), d.bDeviceSubClass,
		    d.bDeviceProtocol, d.idVendor, d.idProduct, d.bcdDevice,
		    d.Manufacturer, d.Product, d.SerialNumber);
		return 0;
	}
	case USBG_IOC_ADD_CONFIGURATION: {
		usbg_ioctl_add_configuration d;
		memcpy(&d, data, sizeof(d));
		if (!u_strcheck(d.udc, string_max) ||
		    !u_strcheck(d.name, string_max) ||
		    !u_strcheck(d.Configuration, string_max))
			return DERR(-EFAULT);
		auto u{gadget::udc::find(d.udc)};
		if (!u)
			return DERR(-ENODEV);
		auto c{std::dynamic_pointer_cast<gadget::composite>(u->device())};
		if (!c)
			return DERR(-EINVAL);
		return c->add_configuration(d.name, d.Configuration,
		    d.bmAttributes, d.bMaxPower);
	}
	case USBG_IOC_ADD_FUNCTION: {
		usbg_ioctl_add_function d;
		memcpy(&d, data, sizeof(d));
		if (!u_strcheck(d.udc, string_max) ||
		    !u_strcheck(d.configs, string_max) ||
		    !u_strcheck(d.function, string_max) ||
		    !u_strcheck(d.data, string_max))
			return DERR(-EFAULT);
		auto u{gadget::udc::find(d.udc)};
		if (!u)
			return DERR(-ENODEV);
		auto c{std::dynamic_pointer_cast<gadget::composite>(u->device())};
		if (!c)
			return DERR(-EINVAL);
		auto f{gadget::function::instantiate(*u, d.function)};
		if (!f.get())
			return DERR(-EINVAL);
		if (auto r{f->configure(d.data)}; r < 0)
			return r;
		return c->add_function(d.configs, std::move(f));
	}
	case USBG_IOC_START: {
		auto n{static_cast<const char *>(data)};
		if (!u_strcheck(n, string_max))
			return DERR(-EFAULT);
		auto u{gadget::udc::find(n)};
		if (!u)
			return DERR(-ENODEV);
		return u->start();
	}
	case USBG_IOC_STOP: {
		auto n{static_cast<char *>(data)};
		if (!u_strcheck(n, string_max))
			return DERR(-EFAULT);
		auto u{gadget::udc::find(n)};
		if (!u)
			return DERR(-ENODEV);
		u->stop();
		return 0;
	}
	default:
		return DERR(-EINVAL);
	}
}

}

/*
 * usb_gadget_init - create /dev/usbgadget device node
 */
extern "C" void
usb_gadget_init()
{
	static devio io = {
		.open = nullptr,
		.close = nullptr,
		.read = nullptr,
		.write = nullptr,
		.ioctl = gadget_ioctl,
		.event = nullptr,
	};
	device_create(&io, "usbgadget", DF_CHR, NULL);
}
