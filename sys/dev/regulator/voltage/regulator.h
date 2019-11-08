#ifndef dev_regulator_voltage_regulator_h
#define dev_regulator_voltage_regulator_h

/*
 * Generic Voltage Regulator
 */

#include "desc.h"
#include <string>

namespace regulator {

class voltage {
public:
	voltage(std::string_view name);
	virtual ~voltage();

	float get() const;
	int set(float min_voltage, float max_voltage);
	bool supports(float min_voltage, float max_voltage) const;
	bool equal(const voltage *) const;

	const std::string &name() const;

private:
	virtual float v_get() const = 0;
	virtual int v_set(float min_voltage, float max_voltage) = 0;
	virtual bool v_supports(float voltage_min, float voltage_max) const = 0;

	const std::string name_;

public:
	static void add(voltage *);
	static voltage *find(std::string_view name);
	static voltage *bind(const volt_reg_desc &);
};

}

#endif
