#include "regulator.h"

#include <debug.h>
#include <list>
#include <sync.h>

namespace regulator {

/*
 * voltage::voltage
 */
voltage::voltage(std::string_view name)
: name_{name}
{ }

/*
 * voltage::~voltage
 */
voltage::~voltage()
{ }

/*
 * voltage::get - get current voltage setting of regulator.
 */
float
voltage::get() const
{
	return v_get();
}

/*
 * voltage::set - set output voltage of regulator to a supported value between
 *		  voltage_min and voltage_max.
 *
 * If a suitable voltage cannot be set an error code will returned and the
 * current voltage setting will be unchanged.
 */
int
voltage::set(float min_voltage, float max_voltage)
{
	return v_set(min_voltage, max_voltage);
}

/*
 * voltage::supports - check if regulator supports an output voltage between
 *		       voltage_min and voltage_max.
 *
 * Returns true if a suitable voltage is supported.
 */
bool
voltage::supports(float min_voltage, float max_voltage) const
{
	return v_supports(min_voltage, max_voltage);
}

/*
 * voltage::equal - check if two regulator references refer to the same
 *		    regulator.
 */
bool
voltage::equal(const voltage *r) const
{
	return this == r;
}

/*
 * voltage::name - return name of voltage regulator.
 */
const std::string &
voltage::name() const
{
	return name_;
}

namespace {

/*
 * Voltage regulator list
 */
a::spinlock regulators_lock;
std::list<voltage *> regulators;

}

/*
 * voltage::add
 */
void
voltage::add(voltage *v)
{
	std::lock_guard l{regulators_lock};

	for (auto e : regulators) {
		if (e->name() == v->name()) {
			dbg("regulators::voltage::add: %s duplicate regulator\n",
			    v->name().c_str());
			return;
		}
	}
	regulators.emplace_back(v);
}

/*
 * voltage::find
 */
voltage *
voltage::find(std::string_view name)
{
	std::lock_guard l{regulators_lock};

	for (auto v : regulators)
		if (v->name() == name)
			return v;
	return nullptr;
}

/*
 * voltage::bind
 */
voltage *
voltage::bind(const volt_reg_desc &desc)
{
	return find(desc.name);
}

}
