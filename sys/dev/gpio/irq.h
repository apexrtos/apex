#ifndef dev_gpio_irq_h
#define dev_gpio_irq_h

/*
 * GPIO IRQ Modes
 */

enum GPIO_IRQ_MODE {
	GPIO_IRQ_MODE_EDGE_RISING,
	GPIO_IRQ_MODE_EDGE_FALLING,
	GPIO_IRQ_MODE_EDGE_BOTH,
	GPIO_IRQ_MODE_HIGH,
	GPIO_IRQ_MODE_LOW,
};

#ifdef __cplusplus
namespace gpio {

enum class irq_mode {
	edge_rising,
	edge_falling,
	edge_both,
	high,
	low,
};

}
#endif

#endif
