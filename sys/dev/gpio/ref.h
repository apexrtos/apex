#ifndef dev_gpio_ref_h
#define dev_gpio_ref_h

/*
 * GPIO Pin Reference
 */

#include "desc.h"
#include "irq.h"
#include <cstddef>
#include <memory>

namespace gpio {

class controller;

class ref {
public:
	using isr = void (*)(unsigned, void *);

	ref(controller *, size_t pin);

	bool get();
	void set(bool);
	void direction_input();
	void direction_output();
	int irq_attach(irq_mode, isr, void *data);
	void irq_detach();
	void irq_mask();
	void irq_unmask();

private:
	controller *c_;
	size_t pin_;

public:
	static std::unique_ptr<ref> bind(const gpio_desc &);
};

}

#endif
