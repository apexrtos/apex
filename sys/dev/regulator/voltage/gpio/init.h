#pragma once

/*
 * Driver for GPIO voltage regulator
 */

#include <dev/gpio/desc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct regulator_voltage_gpio_desc {
	const char *name;
	struct gpio_desc gpios[4];
	struct {
		unsigned long gpio_bitfield;
		float voltage;
	} states[16];
	unsigned startup;
};

void regulator_voltage_gpio_init(const struct regulator_voltage_gpio_desc *);

#ifdef __cplusplus
}
#endif
