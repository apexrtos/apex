#include "voltage.h"

#include "regulator.h"

extern "C" {

struct volt_reg : public regulator::voltage { };

/*
 * volt_reg_bind - bind regulator reference to regulator description.
 */
volt_reg *
volt_reg_bind(const volt_reg_desc *desc)
{
	return static_cast<volt_reg *>(regulator::voltage::bind(*desc));
}

/*
 * volt_reg_get - get current voltage setting of regulator.
 */
float
volt_reg_get(const volt_reg *r)
{
	return r->get();
}

/*
 * volt_reg_set - set output voltage of regulator to a supported value between
 *		  voltage_min and voltage_max.
 *
 * If a suitable voltage cannot be set an error code will returned and the
 * current voltage setting will be unchanged.
 */
int
volt_reg_set(volt_reg *r, float voltage_min, float voltage_max)
{
	return r->set(voltage_min, voltage_max);
}

/*
 * volt_reg_supports - check if regulator supports an output voltage between
 *		       voltage_min and voltage_max.
 *
 * If a suitable voltage is not supported an error code will be returned.
 */
int
volt_reg_supports(const volt_reg *r, float min_voltage, float max_voltage)
{
	return r->supports(min_voltage, max_voltage);
}

/*
 * volt_reg_equal - check if two regulator references refer to the same
 *		    regulator.
 */
bool
volt_reg_equal(const volt_reg *l, const volt_reg *r)
{
	return l->equal(r);
}

}
