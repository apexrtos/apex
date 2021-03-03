#pragma once

/*
 * GPIO Configuration Descriptor
 */

struct gpio_desc {
	const char *controller;	    /* name of GPIO controller */
	unsigned long pin;	    /* pin offset on GPIO controller */
};
