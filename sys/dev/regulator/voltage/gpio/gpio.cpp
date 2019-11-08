#include "init.h"

#include <cmath>
#include <debug.h>
#include <dev/gpio/ref.h>
#include <dev/regulator/voltage/regulator.h>
#include <errno.h>
#include <sync.h>
#include <vector>

namespace {

/*
 * gpio_reg
 */
class gpio_reg final : public regulator::voltage {
public:
	gpio_reg(const regulator_voltage_gpio_desc *d);

private:
	float v_get() const override;
	int v_set(float, float) override;
	bool v_supports(float, float) const override;

	int find(float, float) const;

	struct state {
		unsigned long gpio_bitfield;
		float voltage;
	};

	std::vector<std::unique_ptr<gpio::ref>> gpios_;
	std::vector<state> states_;
	size_t state_;
};

/*
 * gpio_reg::gpio_reg
 */
gpio_reg::gpio_reg(const regulator_voltage_gpio_desc *d)
: regulator::voltage(d->name)
{
	for (auto &p : d->gpios) {
		if (!p.controller)
			break;
		auto r = gpio::ref::bind(p);
		if (!r)
			panic("bad desc");
		gpios_.emplace_back(std::move(r));
	}

	auto v = -std::numeric_limits<float>::infinity();
	for (auto &p : d->states) {
		/* voltages must increase */
		if (p.voltage <= v)
			break;
		states_.push_back({p.gpio_bitfield, p.voltage});
	}

	assert(d->startup < states_.size());
	const auto startup_v = states_[d->startup].voltage;
	if (set(startup_v, startup_v))
		panic("bad desc");

	/* set direction after initialising to reduce glitching */
	for (auto &g : gpios_)
		g->direction_output();
}

/*
 * gpio_reg::v_get
 */
float
gpio_reg::v_get() const
{
	return states_[state_].voltage;
}

/*
 * gpio_reg::v_set
 */
int
gpio_reg::v_set(float voltage_min, float voltage_max)
{
	const auto idx = find(voltage_min, voltage_max);

	if (idx < 0)
		return -ENOTSUP;

	const auto &s = states_[idx];

	dbg("%s: setting to %dmV\n", name().c_str(), (int)(s.voltage * 1000));

	const auto sz = gpios_.size();
	for (size_t i = 0; i < sz; ++i)
		gpios_[i]->set(s.gpio_bitfield & 1ul << (sz - i - 1));
	state_ = idx;

	return 0;
}

/*
 * gpio_reg::v_supports
 */
bool
gpio_reg::v_supports(float voltage_min, float voltage_max) const
{
	return find(voltage_min, voltage_max) >= 0;
}

/*
 * gpio_reg::find - find the closest voltage to an ideal voltage halfway
 *		    between voltage_min and voltage_max
 *
 * Returns index into states_ or -1 if no suitable state is available.
 */
int
gpio_reg::find(float voltage_min, float voltage_max) const
{
	const float x = (voltage_min + voltage_max) / 2;

	/* Find closest state to x. */
	const auto s = std::min_element(begin(states_), end(states_),
	    [&](const auto &v, const auto &best) {
		return abs(v.voltage - x) < abs(best.voltage - x);
	});

	/* Make sure closest state meets min/max criterion. */
	if (s->voltage < voltage_min || s->voltage > voltage_max)
		return -1;

	return s - begin(states_);
}

}

/*
 * regulator_voltage_gpio_init
 */
extern "C" void
regulator_voltage_gpio_init(const regulator_voltage_gpio_desc *d)
{
	regulator::voltage::add(new gpio_reg(d));
}
