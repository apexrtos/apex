#ifndef dev_gpio_desc_h
#define dev_gpio_desc_h

/*
 * GPIO Configuration Descriptor
 */

struct gpio_desc {
	const char *controller;	    /* name of GPIO controller */
	unsigned long pin;	    /* pin offset on GPIO controller */
};

#endif
