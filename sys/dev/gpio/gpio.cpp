#include "gpio.h"

#include "ref.h"

extern "C" {

struct gpio_ref : public gpio::ref { };

/*
 * gpio_bind - bind GPIO pin reference to GPIO description.
 */
gpio_ref *
gpio_bind(const gpio_desc *desc)
{
	return static_cast<gpio_ref *>(gpio::ref::bind(desc).release());
}

/*
 * gpio_get - get the input value of referenced GPIO pin.
 *
 * Returns
 * == 0: if the input is low.
 * != 0: if the input is high.
 */
int
gpio_get(gpio_ref *r)
{
	return r->get();
}

/*
 * gpio_set - set the output value of referenced GPIO pin.
 *
 * If value is
 * == 0: the output will be set to its low state.
 * != 0: the output will be set to its high state.
 */
void
gpio_set(gpio_ref *r, int value)
{
	r->set(value);
}

/*
 * gpio_direction_input - configure referenced GPIO pin as an input.
 */
void
gpio_direction_input(gpio_ref *r)
{
	r->direction_input();
}

/*
 * gpio_direction_output - configure referenced GPIO pin as an output.
 */
void
gpio_direction_output(gpio_ref *r)
{
	r->direction_output();
}

/*
 * gpio_irq_attach - attach handler to referenced GPIO pin interrupt.
 */
int
gpio_irq_attach(gpio_ref *r, GPIO_IRQ_MODE mode, int (*isr)(int, void *),
    void *data)
{
	return r->irq_attach(static_cast<gpio::irq_mode>(mode), isr, data);
}

/*
 * gpio_irq_detach - detach handler from referenced GPIO pin interrupt.
 */
void
gpio_irq_detach(gpio_ref *r)
{
	r->irq_detach();
}

}
