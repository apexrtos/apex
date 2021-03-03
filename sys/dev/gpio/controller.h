#pragma once

/*
 * Generic GPIO Controller
 */

#include "irq.h"
#include <string>
#include <sync.h>

namespace gpio {

class controller {
public:
	using isr_fn = void (*)(unsigned, void *);

	controller(std::string_view name, size_t pins);
	virtual ~controller();

	bool get(size_t pin);
	void set(size_t pin, bool);
	void direction_input(size_t pin);
	void direction_output(size_t pin);
	int irq_attach(size_t pin, irq_mode, isr_fn, void *data);
	void irq_detach(size_t pin);
	void irq_mask(size_t pin);
	void irq_unmask(size_t pin);

	const std::string &name() const;
	size_t pins() const;

protected:
	void irq(size_t pin);

private:
	struct irq_entry {
		isr_fn isr;
		void *arg;
	};

	virtual bool v_get(size_t) = 0;
	virtual void v_set(size_t, bool) = 0;
	virtual void v_direction_input(size_t) = 0;
	virtual void v_direction_output(size_t) = 0;
	virtual int v_interrupt_setup(size_t, irq_mode) = 0;
	virtual void v_interrupt_mask(size_t) = 0;
	virtual void v_interrupt_unmask(size_t) = 0;

	const std::string name_;
	const size_t pins_;

	a::spinlock_irq lock_;
	irq_entry *irq_table_;

public:
	static void add(controller *);
	static controller *find(std::string_view name);
};

}
