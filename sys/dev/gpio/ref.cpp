#include "ref.h"

#include "controller.h"

namespace gpio {

/*
 * ref::ref
 */
ref::ref(controller *c, size_t pin)
: c_{c}
, pin_{pin}
{ }

/*
 * ref::get - get the input value of referenced GPIO pin.
 *
 * Returns
 * false: if the input is low
 * true: if the input is high
 */
bool
ref::get()
{
	return c_->get(pin_);
}

/*
 * ref::set - set the output value of the referenced GPIO pin.
 *
 * If value is
 * false: the output will be set to its low state.
 * true: the output will be set to its high state.
 */
void
ref::set(bool value)
{
	c_->set(pin_, value);
}

/*
 * ref::direction_input - configure referenced GPIO pin as an input.
 */
void
ref::direction_input()
{
	c_->direction_input(pin_);
}

/*
 * ref::direction_output - configure referenced GPIO pin as an output.
 */
void
ref::direction_output()
{
	c_->direction_output(pin_);
}

/*
 * ref::irq_attach - attach handler to referenced GPIO pin interrupt.
 */
int
ref::irq_attach(irq_mode mode, isr i, void *data)
{
	return c_->irq_attach(pin_, mode, i, data);
}

/*
 * ref::irq_detach - detach handler from referenced GPIO pin interrupt.
 */
void
ref::irq_detach()
{
	c_->irq_detach(pin_);
}

/*
 * ref::bind - bind GPIO pin reference to GPIO description.
 */
std::unique_ptr<ref>
ref::bind(const gpio_desc *desc)
{
	auto c = controller::find(desc->controller);
	if (!c)
		return nullptr;
	return std::make_unique<ref>(c, desc->pin);
}

}
