#pragma once

/*
 * Driver for GPIO voltage regulator
 */

#include <dev/gpio/desc.h>

struct regulator_voltage_gpio_desc {
	const char *name;
	gpio_desc gpios[4];
	struct {
		unsigned long gpio_bitfield;
		float voltage;
	} states[16];
	unsigned startup;
};

void regulator_voltage_gpio_init(const regulator_voltage_gpio_desc *);
