#pragma once

/*
 * Driver for fixed voltage regulator
 */

struct regulator_voltage_fixed_desc {
	const char *name;
	float voltage;
};

void regulator_voltage_fixed_init(const regulator_voltage_fixed_desc *);
