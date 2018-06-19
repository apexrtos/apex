#ifndef driver_h
#define driver_h

#include <device.h>
#include <sections.h>

/*
 * Register driver initialisation
 *
 * "order" is initialize order.
 * The driver with order 0 is called first.
 */
#define REGISTER_DRIVER(_name, _order, _init) \
	static const struct driver __driver_##name \
	    __attribute__((section(".drivers"#_order), used)) = { \
		.name = _name, \
		.init = _init, \
	}

#endif /* !driver_h */

