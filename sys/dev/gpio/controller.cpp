#include "controller.h"

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <kmem.h>
#include <list>

namespace gpio {

/*
 * controller::controller
 */
controller::controller(std::string_view name, size_t pins)
: name_{name}
, pins_{pins}
{
	const auto s = sizeof(irq_entry) * pins;
	irq_table_ = static_cast<irq_entry *>(kmem_alloc(s, MA_FAST));
	if (!irq_table_)
		panic("OOM");
	memset(irq_table_, 0, s);
}

/*
 * controller::~controller
 */
controller::~controller()
{
	kmem_free(irq_table_);
}

/*
 * controller::get - get the input value of pin on GPIO controller.
 *
 * Returns
 * false: if the input is low.
 * true: if the input is high.
 */
bool
controller::get(size_t pin)
{
	return v_get(pin);
}

/*
 * controller::set - set the output value of pin on GPIO controller.
 *
 * If value is
 * false: the output will be set to its low state.
 * true: the output will be set to its high state.
 */
void
controller::set(size_t pin, bool value)
{
	return v_set(pin, value);
}

/*
 * controller::direction_input - configure pin on GPIO controller as an input.
 */
void
controller::direction_input(size_t pin)
{
	v_direction_input(pin);
}

/*
 * controller::direction_output - configure pin on GPIO controller as an output.
 */
void
controller::direction_output(size_t pin)
{
	v_direction_output(pin);
}

/*
 * controller::irq_attach - attach handler to pin on GPIO controller.
 */
int
controller::irq_attach(size_t pin, irq_mode mode, isr_fn fn, void *data)
{
	std::lock_guard l{lock_};

	if (pin >= pins_)
		return DERR(-EINVAL);
	if (irq_table_[pin].isr)
		return DERR(-EBUSY);
	if (auto r = v_interrupt_setup(pin, mode); r)
		return r;
	v_interrupt_unmask(pin);
	irq_table_[pin] = {fn, data};
	return 0;
}

/*
 * controller::irq_detach - detach handler from pin on GPIO controller.
 */
void
controller::irq_detach(size_t pin)
{
	std::lock_guard l{lock_};
	assert(pin < pins_);
	v_interrupt_mask(pin);
	irq_table_[pin] = {nullptr, nullptr};
}

/*
 * controller::irq_mask - disable interrupts from pin on GPIO controller.
 */
void
controller::irq_mask(size_t pin)
{
	v_interrupt_mask(pin);
}

/*
 * controller::irq_unmask - enable interrupts from pin on GPIO controller.
 */
void
controller::irq_unmask(size_t pin)
{
	v_interrupt_unmask(pin);
}

/*
 * controller::name - returns GPIO controller name.
 */
const std::string &
controller::name() const
{
	return name_;
}

/*
 * controller::pins - returns number of pins on GPIO controller.
 */
size_t
controller::pins() const
{
	return pins_;
}

/*
 * controller::irq - handle IRQ on GPIO controller pin.
 */
void
controller::irq(size_t pin)
{
	assert(pin < pins_);

	const auto s = lock_.lock();
	auto e = irq_table_[pin];
	lock_.unlock(s);

	if (!e.isr)
		return;
	e.isr(pin, e.arg);
}

namespace {

/*
 * GPIO controller list
 */
a::spinlock controllers_lock;
std::list<controller *> controllers;

}

/*
 * controller::add
 */
void
controller::add(controller *c)
{
	std::lock_guard l{controllers_lock};

	for (auto e : controllers) {
		if (e->name() == c->name()) {
			dbg("gpio::controller::add: %s duplicate controller\n",
			    c->name().c_str());
			return;
		}
	}
	controllers.emplace_back(c);
}

/*
 * controller::find
 */
controller *
controller::find(std::string_view name)
{
	std::lock_guard l{controllers_lock};

	for (auto c : controllers)
		if (c->name() == name)
			return c;
	return nullptr;
}

}
