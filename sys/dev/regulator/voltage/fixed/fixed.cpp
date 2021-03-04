#include "init.h"

#include <debug.h>
#include <dev/regulator/voltage/regulator.h>
#include <errno.h>

namespace {

/*
 * fixed
 */
class fixed final : public regulator::voltage {
public:
	fixed(std::string_view name, float voltage);

private:
	float v_get() const override;
	int v_set(float, float) override;
	bool v_supports(float, float) const override;

	const float voltage_;
};

/*
 * fixed::fixed
 */
fixed::fixed(std::string_view name, float voltage)
: regulator::voltage(name)
, voltage_{voltage}
{ }

/*
 * fixed::v_get
 */
float
fixed::v_get() const
{
	return voltage_;
}

/*
 * fixed::v_set
 */
int
fixed::v_set(float min_voltage, float max_voltage)
{
	if (!v_supports(min_voltage, max_voltage))
		return -ENOTSUP;
	return 0;
}

/*
 * fixed::v_supports
 */
bool
fixed::v_supports(float min_voltage, float max_voltage) const
{
	return voltage_ >= min_voltage && voltage_ <= max_voltage;
}

}

/*
 * regulator_voltage_fixed_init
 */
void
regulator_voltage_fixed_init(const regulator_voltage_fixed_desc *d)
{
	regulator::voltage::add(new fixed(d->name, d->voltage));
}
