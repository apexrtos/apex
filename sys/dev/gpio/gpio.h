#ifndef dev_gpio_gpio_h
#define dev_gpio_gpio_h

/*
 * Generic GPIO support
 */

#include "desc.h"
#include "irq.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gpio_ref;

struct gpio_ref *gpio_bind(const struct gpio_desc *);
int gpio_get(struct gpio_ref *);
void gpio_set(struct gpio_ref *, int value);
void gpio_direction_input(struct gpio_ref *);
void gpio_direction_output(struct gpio_ref *);
int gpio_irq_attach(struct gpio_ref *, enum GPIO_IRQ_MODE,
		    int (*isr)(int, void *), void *data);
void gpio_irq_detach(struct gpio_ref *);

#ifdef __cplusplus
}
#endif

#endif
